// Snake.Server/Rooms/GameRoom.cs
using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using Snake.Server.Core;
using Snake.Server.Data;
using Snake.Server.Services;
using Snake.Shared;
using System.Drawing;

namespace Snake.Server.Rooms;

public class GameRoom
{
    private readonly string _roomId;
    private readonly IHubContext<GameHub> _hub;
    private readonly IDbContextFactory<AppDbContext> _dbFactory;
    private readonly RewardService _rewards;
    private readonly SnapshotMapper _mapper;

    private readonly int _tickRate, _minPlayers, _waitingTicksInit, _playingTicksInit;
    private readonly int _boardW, _boardH;

    private GameCore _core;
    private MatchPhase _phase = MatchPhase.Waiting;
    private int _phaseTicksRemaining;

    private readonly Queue<InputCommand> _inputs = new();
    private readonly Dictionary<string, string> _playerConn = new(); // PlayerId -> ConnId
    private readonly Dictionary<string, string> _connPlayer = new(); // ConnId -> PlayerId
    private readonly Dictionary<string, (int Level, int Xp, string Name)> _account = new();

    private int? _currentMatchId;
    private readonly List<MovementLog> _pendingLogs = new();
    private const int _logFlushInterval = 30;
    private readonly HashSet<string> _ready = new();

    private readonly Dictionary<string, string> _connIp = new();   // ConnId -> IP
    private readonly Dictionary<string, string> _nameConn = new(); // PlayerName -> ConnId
    private string? _hostPlayerId;                                  // 방장

    public int PlayerCount => _core.Players.Count();

    public GameRoom(
        IHubContext<GameHub> hub,
        IDbContextFactory<AppDbContext> dbFactory,
        IConfiguration cfg,
        RewardService rewards,
        SnapshotMapper mapper,
        string roomId)
    {
        _hub = hub;
        _dbFactory = dbFactory;
        _rewards = rewards;
        _mapper = mapper;
        _roomId = roomId;

        _tickRate = cfg.GetValue<int>("Game:TickRate", 12);
        _boardW = cfg.GetValue<int>("Game:BoardWidth", 48);
        _boardH = cfg.GetValue<int>("Game:BoardHeight", 32);
        _minPlayers = cfg.GetValue<int>("Game:MinPlayersToStart", 2);
        _waitingTicksInit = cfg.GetValue<int>("Game:WaitingPhaseTicks", 120);
        _playingTicksInit = cfg.GetValue<int>("Game:PlayingPhaseTicks", 1800);

        _phaseTicksRemaining = _waitingTicksInit;
        _core = new GameCore(new BoardSize(_boardW, _boardH));
    }

    public async Task<JoinAccepted> JoinAsync(string connId, string remoteIp, JoinRequest req)
    {
        using var db = await _dbFactory.CreateDbContextAsync();
        var entity = await db.Players.FirstOrDefaultAsync(p => p.Name == req.PlayerName);
        if (entity is null)
        {
            entity = new Snake.Server.Data.Player { Name = req.PlayerName, Level = 1, Xp = 0, Coins = 0 };
            db.Players.Add(entity);
            await db.SaveChangesAsync();
        }

        // 같은 IP/닉 중복 접속 방지
        var oldByIp = _connIp.FirstOrDefault(kv => kv.Value == remoteIp);
        if (!string.IsNullOrEmpty(oldByIp.Key))
        {
            await _hub.Clients.Client(oldByIp.Key).SendAsync("ForceDisconnected", "Same IP re-login detected");
            LeaveByConnection(oldByIp.Key, "ip-dup");
        }
        if (_nameConn.TryGetValue(entity.Name, out var oldConn))
        {
            await _hub.Clients.Client(oldConn).SendAsync("ForceDisconnected", "Same name re-login detected");
            LeaveByConnection(oldConn, "name-dup");
        }

        var playerId = $"{_roomId}:{Guid.NewGuid():N}";
        _playerConn[playerId] = connId;
        _connPlayer[connId] = playerId;
        _connIp[connId] = remoteIp;
        _nameConn[entity.Name] = connId;
        _account[playerId] = (entity.Level, entity.Xp, entity.Name);
        _ready.Remove(playerId);

        // 첫 입장자를 호스트로
        _hostPlayerId ??= playerId;

        // 그룹 가입
        await _hub.Groups.AddToGroupAsync(connId, _roomId);


        // 스킨
        var selectedId = !string.IsNullOrWhiteSpace(req.SelectedCosmeticId) ? req.SelectedCosmeticId : entity.SelectedCosmeticId;
        var cosmetic = !string.IsNullOrWhiteSpace(selectedId)
            ? Snake.Shared.CosmeticCatalog.InstanceOf(selectedId!)
            : RandomOrUnlockedCosmetic(entity.Level);

        _core.AddPlayer(playerId, entity.Name, entity.Level, cosmetic);

        await BroadcastRoster();
        await _hub.Clients.Group(_roomId).SendAsync("RoomSystem", $"{entity.Name} 님이 입장했습니다.");

        return new JoinAccepted(JoinResult.Ok, playerId, _roomId, _tickRate, cosmetic, entity.Level, entity.Xp, selectedId);
    }

    public Task EnqueueInput(InputCommand cmd) { _inputs.Enqueue(cmd); return Task.CompletedTask; }

    public async void LeaveByConnection(string connId, string reason)
    {
        if (_connPlayer.TryGetValue(connId, out var pid))
        {
            _connPlayer.Remove(connId);
            _playerConn.Remove(pid);
            _ready.Remove(pid);

            _connIp.Remove(connId);
            var name = _account.TryGetValue(pid, out var acc) ? acc.Name : null;
            if (!string.IsNullOrEmpty(name) && _nameConn.TryGetValue(name!, out var cur) && cur == connId)
                _nameConn.Remove(name!);

            if (_hostPlayerId == pid)
                _hostPlayerId = _core.Players.Select(p => p.Id).FirstOrDefault();

            _core.RemovePlayer(pid);

            await _hub.Groups.RemoveFromGroupAsync(connId, _roomId);
            await BroadcastRoster();
            await _hub.Clients.Group(_roomId).SendAsync("RoomSystem", $"{name ?? "알 수 없음"} 님이 퇴장했습니다.");
        }
    }

    // 호스트 여부
    public bool IsHost(string playerId) => _hostPlayerId == playerId;

    // 호스트 시작
    public async Task<bool> TryStartByHostAsync(string playerId)
    {
        if (!IsHost(playerId)) return false;

        var players = _core.Players.Count();
        var readyCt = _ready.Count;
        if (players >= _minPlayers && readyCt >= Math.Min(_minPlayers, players))
        {
            await StartMatchAsync();
            _phase = MatchPhase.Playing;
            return true;
        }
        return false;
    }

    public async Task TickAsync()
    {
        while (_inputs.TryDequeue(out var input))
            _core.ApplyInput(input.PlayerId, input.Key);

        switch (_phase)
        {
            case MatchPhase.Waiting:
                // 수동 시작 대기
                break;

            case MatchPhase.Playing:
                _core.Update();
                _phaseTicksRemaining--;

                if (_currentMatchId.HasValue)
                {
                    foreach (var p in _core.Players)
                    {
                        var head = p.Body.FirstOrDefault();
                        _pendingLogs.Add(new MovementLog
                        {
                            MatchId = _currentMatchId.Value,
                            PlayerName = p.Name,
                            Tick = _core.Tick,
                            X = head.X,
                            Y = head.Y,
                            Alive = p.Alive,
                            Score = p.Score,
                            Utc = DateTime.UtcNow
                        });
                    }
                    if (_core.Tick % _logFlushInterval == 0)
                        await FlushLogsAsync();
                }

                var alive = _core.Players.Count(p => p.Alive);
                if (_phaseTicksRemaining <= 0 || alive <= 1)
                {
                    await FinishMatchAsync();
                    _phase = MatchPhase.Finished;
                    _phaseTicksRemaining = 180;
                }
                break;

            case MatchPhase.Finished:
                _phaseTicksRemaining--;
                if (_phaseTicksRemaining <= 0) ResetRoomForNextMatch();
                break;
        }

        var snap = _mapper.Build(_roomId, _core, _phase, Math.Max(0, _phaseTicksRemaining), _account);
        foreach (var (pid, conn) in _playerConn)
            await _hub.Clients.Client(conn).SendAsync("Snapshot", snap);
    }

    private async Task StartMatchAsync()
    {
        using var db = await _dbFactory.CreateDbContextAsync();

        var m = new Match { RoomId = _roomId, StartedAt = DateTime.UtcNow, EndedAt = DateTime.UtcNow };
        db.Matches.Add(m);
        await db.SaveChangesAsync();
        _currentMatchId = m.Id;

        _phaseTicksRemaining = _playingTicksInit;
    }

    private async Task FinishMatchAsync()
    {
        using var db = await _dbFactory.CreateDbContextAsync();

        var ranking = _core.Players
            .OrderByDescending(p => p.Alive)
            .ThenByDescending(p => p.Score)
            .ThenByDescending(p => p.Body.Count)
            .Select((p, idx) => (p, Rank: idx + 1)).ToList();

        if (_currentMatchId.HasValue)
        {
            var match = await db.Matches.FirstAsync(x => x.Id == _currentMatchId.Value);
            match.EndedAt = DateTime.UtcNow;

            foreach (var (p, rank) in ranking)
            {
                var acc = await db.Players.FirstAsync(x => x.Name == p.Name);
                var (xp, coins) = _rewards.Calculate(rank, p.Score);
                var nextLevel = acc.Level + (acc.Xp + xp) / 100;
                var unlock = _rewards.TryUnlock(nextLevel);

                acc.Xp += xp;
                while (acc.Xp >= 100) { acc.Level++; acc.Xp -= 100; }
                acc.Coins += coins;

                db.MatchPlayers.Add(new MatchPlayer
                {
                    MatchId = match.Id,
                    PlayerName = acc.Name,
                    Rank = rank,
                    Score = p.Score,
                    Length = p.Body.Count
                });

                if (unlock is not null && !await db.PlayerCosmetics.AnyAsync(pc => pc.PlayerName == acc.Name && pc.CosmeticId == unlock))
                {
                    db.PlayerCosmetics.Add(new PlayerCosmetic { PlayerName = acc.Name, CosmeticId = unlock });
                }
            }

            await FlushLogsAsync();
            await db.SaveChangesAsync();

            var rewards = new List<RankRewardDto>();
            foreach (var (p, rank) in ranking)
            {
                var acc = await db.Players.FirstAsync(x => x.Name == p.Name);
                var unlocked = await db.PlayerCosmetics
                    .Where(pc => pc.PlayerName == acc.Name)
                    .OrderByDescending(pc => pc.Id).FirstOrDefaultAsync();

                var (xp, coins) = _rewards.Calculate(rank, p.Score);
                rewards.Add(new RankRewardDto(GetPidByName(p.Name), p.Name, rank, xp, coins, unlocked != null, unlocked?.CosmeticId));
            }

            foreach (var (pid, conn) in _playerConn)
            {
                var name = _account[pid].Name;
                var acc = await db.Players.FirstAsync(x => x.Name == name);
                await _hub.Clients.Client(conn).SendAsync("MatchFinished",
                    new MatchFinishedDto(_roomId, _core.Tick, rewards, acc.Level, acc.Xp));
            }
        }
    }

    private void ResetRoomForNextMatch()
    {
        var keep = _core.Players.Select(p => (p.Id, p.Name, p.Level, p.Cosmetic)).ToList();
        _core = new GameCore(new BoardSize(_boardW, _boardH));
        foreach (var t in keep)
            _core.AddPlayer(t.Id, t.Name, t.Level, t.Cosmetic);

        _ready.Clear();
        _phase = MatchPhase.Waiting;
        _phaseTicksRemaining = _waitingTicksInit;
        _currentMatchId = null;
        _pendingLogs.Clear();
    }

    private async Task FlushLogsAsync()
    {
        if (_pendingLogs.Count == 0) return;
        using var db = await _dbFactory.CreateDbContextAsync();
        db.MovementLogs.AddRange(_pendingLogs);
        _pendingLogs.Clear();
        await db.SaveChangesAsync();
    }

    private string GetPidByName(string name) =>
        _playerConn.FirstOrDefault(kv => _account.TryGetValue(kv.Key, out var t) && t.Name == name).Key ?? "";

    private CosmeticInstance RandomOrUnlockedCosmetic(int level)
    {
        var pool = CosmeticsHelper.CatalogForLevel(level);
        var id = pool[RandomX.Next(0, pool.Count)];
        return Snake.Shared.CosmeticCatalog.InstanceOf(id);
    }

    public void SetReady(string playerId, bool ready)
    {
        if (ready) _ready.Add(playerId);
        else _ready.Remove(playerId);
    }

    public RoomBrief Brief()
        => new RoomBrief(_roomId, _core.Players.Count, _ready.Count, _phase);

    public bool ApplyCosmeticForName(string playerName, string cosmeticId)
    {
        var cos = Snake.Shared.CosmeticCatalog.InstanceOf(cosmeticId);
        foreach (var p in _core.Players)
            if (string.Equals(p.Name, playerName, StringComparison.OrdinalIgnoreCase))
            {
                p.Cosmetic = cos;      // ★ 즉시 교체
                _ = BroadcastRoster(); // (선택) 명단 갱신
                return true;
            }
        return false;
    }


    // ───────────────────────────────────────────────────────────────────
    // 방장 위임
    // ───────────────────────────────────────────────────────────────────

    public bool DelegateHost(string hostPlayerId, string targetName)
    {
        if (!IsHost(hostPlayerId)) return false;

        var targetPid = _playerConn.FirstOrDefault(kv =>
        {
            var name = _account.TryGetValue(kv.Key, out var a) ? a.Name : null;
            return string.Equals(name, targetName, StringComparison.OrdinalIgnoreCase);
        }).Key;

        if (string.IsNullOrEmpty(targetPid)) return false;

        _hostPlayerId = targetPid;
        _ = BroadcastRoster();                 // 👑 표시 갱신
        _ = _hub.Clients.Group(_roomId).SendAsync("RoomSystem", $"방장이 {targetName} 님으로 위임되었습니다.");
        return true;
    }


    // ───────────────────────────────────────────────────────────────────
    // 방장 강퇴
    // ───────────────────────────────────────────────────────────────────
    public bool KickByName(string hostPlayerId, string targetName)
    {
        if (!IsHost(hostPlayerId)) return false;

        var pid = _playerConn.FirstOrDefault(kv =>
        {
            var name = _account.TryGetValue(kv.Key, out var a) ? a.Name : null;
            return string.Equals(name, targetName, StringComparison.OrdinalIgnoreCase);
        }).Key;

        if (string.IsNullOrEmpty(pid)) return false;

        if (_playerConn.TryGetValue(pid, out var connId))
        {
            _ = _hub.Clients.Client(connId).SendAsync("ForceDisconnected", "Kicked by host");
            LeaveByConnection(connId, "kick");
            return true;
        }
        return false;
    }

    public bool SkipRemain(string hostPlayerId)
    {
        if (!IsHost(hostPlayerId)) return false;

        // 종료 대기(Finished)면 즉시 다음 라운드로
        if (_phase == MatchPhase.Finished)
        {
            _phaseTicksRemaining = 0; // TickAsync에서 ResetRoomForNextMatch()로 진행
            return true;
        }

        // 대기(Waiting)면 '시작 조건'을 바로 검사해 시작 시도
        if (_phase == MatchPhase.Waiting)
        {
            // 호스트 강제 시작과 동일한 동작
            _ = TryStartByHostAsync(hostPlayerId);
            return true;
        }

        // 진행 중(Playing)은 스킵 불가
        return false;
    }
    private async Task BroadcastRoster()
    {
        var list = _core.Players
            .Select(p => new { playerId = p.Id, isHost = (p.Id == _hostPlayerId), name = p.Name })
            .ToList();

        await _hub.Clients.Group(_roomId).SendAsync("RoomRoster", list);
    }
}

// 내부 헬퍼
static class CosmeticsHelper
{
    public static List<string> CatalogForLevel(int level)
    {
        var ids = Snake.Shared.CosmeticCatalog.UnlockByLevel
            .Where(x => x.MinLevel <= level)
            .Select(x => x.Id).ToList();
        if (ids.Count == 0) ids.Add("skin_basic_blue");
        return ids;
    }
}
