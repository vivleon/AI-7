// Snake.Server/GameHub.cs
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;
using Snake.Server.Data;
using Snake.Server.Rooms;
using Snake.Server.Services;
using Snake.Shared;

namespace Snake.Server
{
    public class GameHub : Hub
    {
        // --- 연결 확인용 핑 ---
        public Task<bool> Ping() => Task.FromResult(true);

        private readonly RoomManager _rooms;
        private readonly IDbContextFactory<AppDbContext> _dbFactory;
        private readonly ShopService _shop;

        private static readonly ConcurrentDictionary<string, bool> _reservedRooms = new(StringComparer.OrdinalIgnoreCase);
        private static readonly ConcurrentDictionary<string, string> _privateRoomPwHash = new(StringComparer.OrdinalIgnoreCase);
        private static readonly ConcurrentDictionary<string, string> _hostByRoom = new(StringComparer.OrdinalIgnoreCase); // roomId -> playerId
        private static readonly ConcurrentDictionary<string, string> _playerRoomById = new(); // playerId -> roomId
        private static readonly ConcurrentDictionary<string, string> _connToPlayer = new();   // connectionId -> playerId

        // 접속(ConnectionId)→닉네임 매핑 (로그인 성공 시 채움)
        private static readonly ConcurrentDictionary<string, string> _connToName = new();

        // playerId→닉네임 매핑 (방 입장 시 채움)
        private static readonly ConcurrentDictionary<string, string> _nameByPlayerId = new();

        public GameHub(RoomManager rooms, IDbContextFactory<AppDbContext> dbFactory, ShopService shop)
        {
            _rooms = rooms;
            _dbFactory = dbFactory;
            _shop = shop;
        }

        // ── 유틸
        private static string Sha256(string s)
        {
            using var sha = SHA256.Create();
            var hash = sha.ComputeHash(Encoding.UTF8.GetBytes(s));
            return BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
        }
        private string ClientIp() => Context.GetHttpContext()?.Connection.RemoteIpAddress?.ToString() ?? "0.0.0.0";

        // 현재 roomId의 호스트가 사라졌을 때 다음 플레이어로 승격
        private void PromoteNextHost(string roomId)
        {
            // 방에 실제 남아있는 playerId만 수집
            var players = _playerRoomById
                .Where(kv => kv.Value.Equals(roomId, StringComparison.OrdinalIgnoreCase))
                .Select(kv => kv.Key)
                .ToList();

            if (players.Count == 0)
            {
                _hostByRoom.TryRemove(roomId, out _);
                return;
            }

            // 안정적 승계: 닉네임(알파벳) → playerId(Ordinal) 2차키로 정렬
            var nextPid = players
                .OrderBy(pid => _nameByPlayerId.TryGetValue(pid, out var n) ? n : "\uFFFF", StringComparer.OrdinalIgnoreCase)
                .ThenBy(pid => pid, StringComparer.Ordinal)
                .First();

            _hostByRoom[roomId] = nextPid;
        }


        // ─────────────────────────────────────────────────────────────────────
        // 계정: 회원가입 / 로그인 / 닉네임 변경
        // ─────────────────────────────────────────────────────────────────────
        public async Task<PurchaseResultDto> Register(RegisterDto dto)
        {
            var loginId = (dto.LoginId ?? "").Trim();
            var password = dto.Password ?? "";
            var passwordConfirm = dto.PasswordConfirm ?? "";
            var nick = (dto.Nick ?? "").Trim();
            var name = (dto.Name ?? "").Trim();
            var gender = (dto.Gender ?? "").Trim();

            await using var db = await _dbFactory.CreateDbContextAsync();
            if (await db.Accounts.AnyAsync(a => a.LoginId == loginId))
                return new PurchaseResultDto(false, "이미 존재하는 아이디입니다.");
            if (await db.Accounts.AnyAsync(a => a.Nick == nick))
                return new PurchaseResultDto(false, "이미 존재하는 닉네임입니다.");
            if (string.IsNullOrWhiteSpace(password) || password.Length < 4)
                return new PurchaseResultDto(false, "비밀번호는 4자 이상이어야 합니다.");
            if (!string.Equals(password, passwordConfirm, StringComparison.Ordinal))
                return new PurchaseResultDto(false, "비밀번호 확인이 일치하지 않습니다.");

            db.Accounts.Add(new Account
            {
                LoginId = loginId,
                PasswordHash = Sha256(password),
                Nick = nick,
                Name = name,
                Gender = gender
            });
            if (!await db.Players.AnyAsync(p => p.Name == nick))
                db.Players.Add(new Player { Name = nick, Level = 1, Xp = 0, Coins = 0 });

            await db.SaveChangesAsync();
            return new PurchaseResultDto(true, "가입 완료");
        }

        public async Task<LoginResultDto> Login(string? loginId, string? password)
        {
            var id = (loginId ?? "").Trim();
            var pw = password ?? "";

            await using var db = await _dbFactory.CreateDbContextAsync();
            var acc = await db.Accounts.FirstOrDefaultAsync(a => a.LoginId == id);
            if (acc is null || acc.PasswordHash != Sha256(pw))
                throw new HubException("아이디/비밀번호가 올바르지 않습니다.");

            var prof = await db.Players.FirstAsync(p => p.Name == acc.Nick);
            var profile = new AccountProfileDto(
                prof.Level, prof.Xp, prof.Coins,
                prof.SelectedCosmeticId, prof.SelectedThemeId,
                prof.EmojiTag
            );

            // 이 연결이 어느 닉으로 로그인했는지 기록
            _connToName[Context.ConnectionId] = acc.Nick;

            return new LoginResultDto(acc.Nick, profile);

            
        }

        public async Task<PurchaseResultDto> ChangeNick(string? loginId, string? currentPassword, string? newNick)
        {
            var id = (loginId ?? "").Trim();
            var curPw = currentPassword ?? "";
            var newN = (newNick ?? "").Trim();

            await using var db = await _dbFactory.CreateDbContextAsync();
            var acc = await db.Accounts.FirstOrDefaultAsync(a => a.LoginId == id);
            if (acc is null) return new(false, "계정을 찾을 수 없습니다.", 0);
            if (acc.PasswordHash != Sha256(curPw)) return new(false, "비밀번호가 올바르지 않습니다.", 0);

            if (newN.Length < 2 || newN.Length > 16) return new(false, "닉네임은 2~16자", 0);
            if (await db.Accounts.AnyAsync(a => a.Nick == newN)) return new(false, "이미 존재하는 닉네임", 0);

            var player = await db.Players.FirstOrDefaultAsync(p => p.Name == acc.Nick);
            if (player is null) return new(false, "플레이어 프로필이 없습니다.", 0);

            acc.Nick = newN;
            player.Name = newN;
            await db.SaveChangesAsync();
            return new(true, "닉네임이 변경되었습니다.", player.Coins);
        }



        // ─────────────────────────────────────────────────────────────────────
        // 플레이어 정보 카드
        // ─────────────────────────────────────────────────────────────────────
        public async Task<PlayerInfoDto> GetPlayerInfo(string name)
        {
            await using var db = await _dbFactory.CreateDbContextAsync();

            var p = await db.Players.AsNoTracking().FirstOrDefaultAsync(x => x.Name == name);
            if (p is null) return new PlayerInfoDto(name, 1, 0, 0, 0);

            var total = await db.MatchPlayers.AsNoTracking().CountAsync(x => x.PlayerName == name);
            var wins = await db.MatchPlayers.AsNoTracking().CountAsync(x => x.PlayerName == name && x.Rank == 1);
            var losses = Math.Max(0, total - wins);

            return new PlayerInfoDto(p.Name, p.Level, p.Xp, wins, losses);
        }

        // ─────────────────────────────────────────────────────────────────────
        // 방/게임
        // ─────────────────────────────────────────────────────────────────────
        public async Task<CreateRoomResult> CreateRoom(CreateRoomRequest req)
        {
            var title = (req.Title ?? "").Trim();
            if (string.IsNullOrWhiteSpace(title))
                return new(false, title, "방 제목을 입력하세요.");

            var list = await _rooms.ListRoomsAsync();
            if (list.Any(r => r.RoomId.Equals(title, StringComparison.OrdinalIgnoreCase)))
                return new(false, title, "이미 존재하는 방입니다.");
            if (_reservedRooms.ContainsKey(title))
                return new(false, title, "이미 존재하는 방입니다.");

            _reservedRooms[title] = req.IsPrivate;
            if (req.IsPrivate && !string.IsNullOrWhiteSpace(req.Password))
                _privateRoomPwHash[title] = Sha256(req.Password!);

            return new(true, title, "방이 생성(예약)되었습니다.");
        }

        public Task<bool> IsPrivateRoom(string roomId) => Task.FromResult(_privateRoomPwHash.ContainsKey(roomId));

        public async Task<List<RoomBrief>> ListRooms()
        {
            var list = await _rooms.ListRoomsAsync();
            var runningSet = new HashSet<string>(list.Select(x => x.RoomId), StringComparer.OrdinalIgnoreCase);
            foreach (var kv in _reservedRooms)
            {
                var title = kv.Key;
                if (!runningSet.Contains(title))
                    list.Add(new RoomBrief(title, 0, 0, MatchPhase.Waiting));
            }
            return list.OrderBy(x => x.RoomId).ToList();
        }

        public async Task<JoinAccepted> JoinRoom(JoinRequest req)
        {
            if (_privateRoomPwHash.TryGetValue(req.RoomId, out var hash))
            {
                var ok = !string.IsNullOrEmpty(req.Password) && Sha256(req.Password!) == hash;
                if (!ok) throw new HubException("비공개 방입니다. 비밀번호가 올바르지 않습니다.");
            }

            var ip = ClientIp();
            var acc = await _rooms.JoinAsync(Context.ConnectionId, ip, req);

            // playerId ↔ 닉네임 연결
            _nameByPlayerId[acc.PlayerId] = req.PlayerName;

            _playerRoomById[acc.PlayerId] = req.RoomId;
            _connToPlayer[Context.ConnectionId] = acc.PlayerId;

            _hostByRoom.TryAdd(req.RoomId, acc.PlayerId);
            await Groups.AddToGroupAsync(Context.ConnectionId, req.RoomId);
            _reservedRooms.TryRemove(req.RoomId, out _);
            await BroadcastRoster(req.RoomId);
            await Clients.Group(req.RoomId).SendAsync("RoomSystem", $"{req.PlayerName} 님이 입장했습니다.");
            return acc;
        }

        public Task SendInput(InputCommand cmd) => _rooms.EnqueueInputAsync(cmd);
        public Task SetReady(string playerId, bool ready) => _rooms.SetReadyAsync(playerId, ready);
        public Task LeaveByConnection(string reason) => _rooms.LeaveByConnectionAsync(Context.ConnectionId, reason);

        public async Task<bool> AmIHost(string playerId)
        {
            var m = _rooms.GetType().GetMethod("AmIHostAsync");
            if (m != null)
            {
                var t = (Task<bool>)m.Invoke(_rooms, new object[] { playerId })!;
                return await t;
            }
            if (!_playerRoomById.TryGetValue(playerId, out var roomId) || string.IsNullOrEmpty(roomId))
                return false;
            return _hostByRoom.TryGetValue(roomId, out var hostId) && hostId == playerId;
        }

        public async Task<bool> StartMatch(string playerId)
        {
            if (!await AmIHost(playerId)) return false;
            var m = _rooms.GetType().GetMethod("StartMatchAsync");
            if (m != null)
            {
                var t = (Task<bool>)m.Invoke(_rooms, new object[] { playerId })!;
                var ok = await t;
                if (ok && _playerRoomById.TryGetValue(playerId, out var rid))
                    await Clients.Group(rid).SendAsync("RoomSystem", "게임이 시작되었습니다.");
                return ok;
            }
            return false;
        }

        // ▶ 방장 전용: 인터미션/종료 대기 즉시 스킵
        public Task<bool> SkipRemain(string roomId, string hostPlayerId)
            => _rooms.SkipRemainAsync(roomId, hostPlayerId);


        public Task<bool> DelegateHost(string roomId, string hostPlayerId, string targetName)
            => _rooms.DelegateHostAsync(roomId, hostPlayerId, targetName);

        public async Task<bool> KickPlayer(string roomId, string targetName, string hostPlayerId)
        {
            // 자기 자신 킥 방지
            if (_nameByPlayerId.TryGetValue(hostPlayerId, out var hostName) &&
                targetName.Equals(hostName, StringComparison.OrdinalIgnoreCase))
                return false;

            // 1) 실제 킥 시도 (RoomManager가 내부 상태를 먼저 업데이트)
            var ok = await _rooms.KickPlayerAsync(roomId, hostPlayerId, targetName);
            if (!ok) return false;

            // 2) 대상 커넥션 찾아서 개별 통지 + 그룹 제거
            var targetConn = _connToName.FirstOrDefault(kv =>
                                kv.Value.Equals(targetName, StringComparison.OrdinalIgnoreCase)).Key;
            if (!string.IsNullOrEmpty(targetConn))
            {
                await Clients.Client(targetConn).SendAsync("ForceDisconnected", "Kicked by host");
                await Groups.RemoveFromGroupAsync(targetConn, roomId);

                // 사내 매핑도 가능한 범위에서 정리
                if (_connToPlayer.TryGetValue(targetConn, out var kickedPid) && !string.IsNullOrEmpty(kickedPid))
                {
                    _playerRoomById.TryRemove(kickedPid, out _);
                    _connToPlayer.TryRemove(targetConn, out _);
                    _nameByPlayerId.TryRemove(kickedPid, out _);
                }
            }

            // 3) 호스트 승계: 이제 강퇴자의 매핑이 제거된 뒤이므로 안전
            var targetPid = _nameByPlayerId.FirstOrDefault(kv => kv.Value.Equals(targetName, StringComparison.OrdinalIgnoreCase)).Key;
            // 위에서 제거 못했더라도 승계 계산에서 제외
            if (_hostByRoom.TryGetValue(roomId, out var curHost) && (curHost == targetPid))
            {
                PromoteNextHost(roomId);
            }

            // 4) 공지 + 최신 명단 1회만 브로드캐스트
            await Clients.Group(roomId).SendAsync("RoomSystem", $"{targetName} 님이 방장에 의해 강퇴되었습니다.");
            await BroadcastRoster(roomId);

            return true;
        }




        public override async Task OnDisconnectedAsync(Exception? exception)
        {
            // 연결↔닉네임, 연결↔playerId 정리
            _connToName.TryRemove(Context.ConnectionId, out _);

            if (_connToPlayer.TryRemove(Context.ConnectionId, out var pid) && !string.IsNullOrEmpty(pid))
            {
                _nameByPlayerId.TryRemove(pid, out _);

                if (_playerRoomById.TryRemove(pid, out var room) && !string.IsNullOrEmpty(room))
                {
                    await Groups.RemoveFromGroupAsync(Context.ConnectionId, room);

                    if (_hostByRoom.TryGetValue(room, out var hostId) && hostId == pid)
                        PromoteNextHost(room);   // 승계

                    await Clients.Group(room).SendAsync("RoomSystem", "플레이어가 퇴장했습니다.");
                    await BroadcastRoster(room);

                    if (_rooms.IsRoomEmpty(room))
                    {
                        _reservedRooms.TryRemove(room, out _);
                        _privateRoomPwHash.TryRemove(room, out _);
                        _hostByRoom.TryRemove(room, out _);
                    }
                }
            }

            await _rooms.LeaveByConnectionAsync(Context.ConnectionId, "disconnect");
            await base.OnDisconnectedAsync(exception);
        }



        // [ADD] 로비/방 통합 온라인 명단 제공
        public async Task<List<OnlinePlayerBrief>> ListOnlinePlayers()
        {
            // 1) 현재 로그인해 있는 닉네임들
            var names = _connToName.Values
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToList();

            // 2) 닉네임별 현재 방/호스트 여부 구성
            var nameToState = new Dictionary<string, (bool inRoom, string? roomId, bool isHost)>(StringComparer.OrdinalIgnoreCase);
            foreach (var kv in _playerRoomById) // kv: playerId -> roomId
            {
                var playerId = kv.Key;
                var roomId = kv.Value;
                if (_nameByPlayerId.TryGetValue(playerId, out var name))
                {
                    var isHost = _hostByRoom.TryGetValue(roomId, out var hostId) && hostId == playerId;
                    nameToState[name] = (true, roomId, isHost);
                }
            }

            // 3) 레벨 정보는 DB에서 한번에 읽기 (스키마 변경 X, ReadOnly)
            await using var db = await _dbFactory.CreateDbContextAsync();
            var lvList = await db.Players
                .Where(p => names.Contains(p.Name))
                .Select(p => new { p.Name, p.Level })
                .ToListAsync();
            var lvMap = lvList.ToDictionary(x => x.Name, x => x.Level, StringComparer.OrdinalIgnoreCase);

            // 4) DTO로 만들어 반환
            var result = names.Select(n =>
            {
                nameToState.TryGetValue(n, out var st);
                var level = lvMap.TryGetValue(n, out var lv) ? lv : 1;
                return new OnlinePlayerBrief(
                    Name: n,
                    Level: level,
                    InRoom: st.inRoom,
                    RoomId: st.roomId,
                    IsHost: st.isHost
                );
            })
            .OrderByDescending(x => x.Level).ThenBy(x => x.Name)
            .ToList();

            return result;
        }


        // ─────────────────────────────────────────────────────────────────────
        // 채팅
        // ─────────────────────────────────────────────────────────────────────
        public Task SendLobbyChat(string? from, string? text)
            => Clients.All.SendAsync("LobbyChat", DateTime.UtcNow, from ?? "", text ?? "");

        // [REPLACE] 강타입 전송
        public Task SendRoomChat(string? roomId, string? from, string? text)
        {
            var dto = new RoomChatDto(
                DateTime.UtcNow.ToString("HH:mm:ss"),
                roomId ?? "",
                from ?? "",
                text ?? ""
            );
            return Clients.Group(dto.RoomId).SendAsync("RoomChat", dto);
        }

        public Task SendWhisper(string? from, string? to, string? text)
        {
            if (string.IsNullOrWhiteSpace(from) || string.IsNullOrWhiteSpace(to) || string.IsNullOrWhiteSpace(text))
                return Task.CompletedTask;
            if (from.Equals(to, StringComparison.OrdinalIgnoreCase))
                return Task.CompletedTask;

            var fromConn = _connToName.FirstOrDefault(kv => kv.Value.Equals(from, StringComparison.OrdinalIgnoreCase)).Key;
            var toConn = _connToName.FirstOrDefault(kv => kv.Value.Equals(to, StringComparison.OrdinalIgnoreCase)).Key;

            var utc = DateTime.UtcNow;
            var tasks = new List<Task>(2);
            if (!string.IsNullOrEmpty(fromConn))
                tasks.Add(Clients.Client(fromConn).SendAsync("Whisper", utc, from, to, text));
            if (!string.IsNullOrEmpty(toConn) && toConn != fromConn)
                tasks.Add(Clients.Client(toConn).SendAsync("Whisper", utc, from, to, text));

            return Task.WhenAll(tasks);
        }



        // ─────────────────────────────────────────────────────────────────────
        // 상점/인벤토리
        // ─────────────────────────────────────────────────────────────────────
        public Task<AccountProfileDto> GetAccountProfile(string? playerName)
            => _shop.GetProfileAsync(playerName ?? "");
        public Task<InventoryDto> GetInventory(string? playerName)
            => _shop.GetInventoryAsync(playerName ?? "");
        public Task<List<ShopItemDto>> GetCosmeticCatalog(string? playerName)
            => _shop.GetCosmeticCatalogAsync(playerName ?? "");
        public Task<List<ShopItemDto>> GetThemeCatalog(string? playerName)
            => _shop.GetThemeCatalogAsync(playerName ?? "");
        public Task<PurchaseResultDto> BuyCosmetic(string? playerName, string id)
            => _shop.BuyCosmeticAsync(playerName ?? "", id);
        public Task<PurchaseResultDto> BuyTheme(string? playerName, string id)
            => _shop.BuyThemeAsync(playerName ?? "", id);

        // 선택(스킨/테마) – 하나로 통합
        public async Task<bool> SetSelection(string? playerName, SetSelectionRequest req)
        {
            var name = playerName ?? "";
            var ok = await _shop.SetSelectionAsync(name, req);
            if (!ok) return false;

            //name → playerId 경유 후 roomId 조회
            var playerId = _nameByPlayerId
                .Where(kv => kv.Value.Equals(name, StringComparison.OrdinalIgnoreCase))
                .Select(kv => kv.Key)
                .FirstOrDefault();

            if (!string.IsNullOrEmpty(playerId) &&
                _playerRoomById.TryGetValue(playerId, out var roomId) &&
                !string.IsNullOrEmpty(roomId))
            {
                if (!string.IsNullOrWhiteSpace(req.CosmeticId))
                    _rooms.ApplyCosmeticForName(roomId, name, req.CosmeticId);

                // (선택) 테마도 방 즉시 반영하려면 유사 훅 추가 지점
            }

            return true;
        }

        public Task<PurchaseResultDto> SetEmoji(string? playerName, EmojiRequest req)
            => _shop.SetEmojiAsync(playerName ?? "", req.Emoji);


        // === 가챠/제작/샤드 ===
        private static readonly ConcurrentDictionary<string, int> _shards = new(StringComparer.OrdinalIgnoreCase);
        private static List<GachaBannerDto> _gachaBanners = new();
        private static DateTime _gachaCachedAtUtc = DateTime.MinValue;
        private static readonly object _gachaLock = new();
        private static readonly Random _rng = new();

        private static void EnsureDefaultBanners()
        {
            lock (_gachaLock)
            {
                if (_gachaBanners.Count > 0 && (DateTime.UtcNow - _gachaCachedAtUtc) < TimeSpan.FromMinutes(10))
                    return;

                var now = DateTime.UtcNow;
                // Snake.Server/GameHub.cs — EnsureDefaultBanners() 안
                _gachaBanners = new List<GachaBannerDto>
                {
                    new("starter", "스타터 픽업", now.AddDays(-1), now.AddDays(30), new()
                    {
                        // Common
                        new("skin_basic_blue",   "블루 스킨",   "Cosmetic", Rarity.Common),
                        new("skin_basic_green",  "그린 스킨",   "Cosmetic", Rarity.Common),
                        new("skin_basic_yellow", "옐로 스킨",   "Cosmetic", Rarity.Common),
                        new("skin_basic_red",    "레드 스킨",   "Cosmetic", Rarity.Common),

                        new("theme_dark",        "다크 테마",   "Theme",    Rarity.Common),
                        new("theme_mint",        "민트 테마",   "Theme",    Rarity.Common),

                        // Rare
                        new("skin_stripe_red",   "스트라이프 레드", "Cosmetic", Rarity.Rare),
                        new("skin_dot_purple",   "닷 퍼플",         "Cosmetic", Rarity.Rare),
                        new("theme_retro",       "레트로 테마",     "Theme",    Rarity.Rare),
                        new("theme_ocean",       "오션 테마",       "Theme",    Rarity.Rare),

                        // Epic
                        new("skin_ice_crystal",  "아이스 크리스탈", "Cosmetic", Rarity.Epic),
                        new("skin_fire_flame",   "파이어 플레임",   "Cosmetic", Rarity.Epic),
                        new("theme_neon",        "네온 테마",       "Theme",    Rarity.Epic),
                        new("theme_forest",      "포레스트 테마",   "Theme",    Rarity.Epic),

                        // Legendary
                        new("skin_dragon",       "드래곤",        "Cosmetic", Rarity.Legendary),
                        new("skin_phoenix",      "피닉스",        "Cosmetic", Rarity.Legendary),
                    }),

                    new("elemental_fest", "엘리멘탈 페스트", now.AddDays(-1), now.AddDays(21), new()
                    {
                        new("skin_glacier",      "글레이셔",       "Cosmetic", Rarity.Rare),
                        new("skin_lava",         "라바",           "Cosmetic", Rarity.Rare),
                        new("skin_thunder",      "썬더",           "Cosmetic", Rarity.Epic),
                        new("skin_borealis",     "보레알리스",     "Cosmetic", Rarity.Epic),
                        new("skin_void_prism",   "보이드 프리즘",  "Cosmetic", Rarity.Legendary),
                        new("theme_glacier",     "글레이셔 테마",  "Theme",    Rarity.Rare),
                        new("theme_lava",        "라바 테마",      "Theme",    Rarity.Rare),
                        new("theme_thunder",     "썬더 테마",      "Theme",    Rarity.Epic),
                        new("theme_borealis",    "오로라 테마",    "Theme",    Rarity.Legendary),
                    }),

                    new("urban_neon", "어반 네온 위크", now.AddDays(-3), now.AddDays(7), new()
                    {
                        new("skin_neon_cyan",    "네온 시안",      "Cosmetic", Rarity.Rare),
                        new("skin_neon_magenta", "네온 마젠타",    "Cosmetic", Rarity.Rare),
                        new("skin_circuit_neon", "네온 서킷",      "Cosmetic", Rarity.Epic),
                        new("skin_cyberpunk",    "사이버펑크",     "Cosmetic", Rarity.Epic),
                        new("theme_cyberpunk",   "사이버펑크 테마", "Theme",    Rarity.Epic),
                        new("theme_vaporwave",   "베이퍼웨이브",   "Theme",    Rarity.Rare),
                        new("theme_prism",       "프리즘 테마",     "Theme",    Rarity.Legendary),
                    }),

                    new("mythic_beasts", "신화의 수호자", now.AddDays(-1), now.AddDays(30), new()
                    {
                        new("skin_dragon_onyx",  "오닉스 드래곤",  "Cosmetic", Rarity.Legendary),
                        new("skin_phoenix_ashen","애션 피닉스",    "Cosmetic", Rarity.Legendary),
                        new("skin_runic",        "루닉",           "Cosmetic", Rarity.Epic),
                        new("skin_phantom",      "팬텀",           "Cosmetic", Rarity.Epic),
                        new("theme_royal",       "로얄 테마",      "Theme",    Rarity.Epic),
                        new("theme_void",        "보이드 테마",    "Theme",    Rarity.Legendary),
                    }),

                    new("seasonal_colors", "시즌 팔레트", now.AddDays(-1), now.AddDays(30), new()
                    {
                        new("skin_sakura",       "사쿠라",         "Cosmetic", Rarity.Rare),
                        new("skin_koi",          "코이",           "Cosmetic", Rarity.Rare),
                        new("skin_prism",        "프리즘",         "Cosmetic", Rarity.Epic),
                        new("skin_pearl",        "펄",            "Cosmetic", Rarity.Epic),
                        new("theme_sakura",      "사쿠라 테마",    "Theme",    Rarity.Rare),
                        new("theme_pastel",      "파스텔 테마",    "Theme",    Rarity.Rare),
                        new("theme_starry",      "스타리 테마",    "Theme",    Rarity.Epic),
                    }),
                };

                _gachaCachedAtUtc = DateTime.UtcNow;
            }
        }

        public Task<List<GachaBannerDto>> GetGachaBanners()
        {
            EnsureDefaultBanners();
            var now = DateTime.UtcNow;
            var live = _gachaBanners.Where(b => b.StartUtc <= now && now <= b.EndUtc)
                                    .OrderBy(b => b.StartUtc).ToList();
            return Task.FromResult(live);
        }

        public Task<ShardInfoDto> GetShards(string? playerName)
        {
            var key = playerName ?? "";
            _shards.TryGetValue(key, out var s);
            return Task.FromResult(new ShardInfoDto(s));
        }

        public async Task<GachaPullResult> PullGacha(GachaPullRequest req)
        {
            EnsureDefaultBanners();
            var banner = _gachaBanners.FirstOrDefault(b => b.BannerId == req.BannerId)
                         ?? throw new HubException("배너가 없습니다.");

            var count = Math.Clamp(req.Count, 1, 100);
            var cost = (count == 10) ? 450 : 50 * count;

            // 코인 차감
            await using (var db = await _dbFactory.CreateDbContextAsync())
            {
                var p = await db.Players.FirstOrDefaultAsync(x => x.Name == req.PlayerName)
                        ?? throw new HubException("플레이어를 찾을 수 없습니다.");
                if (p.Coins < cost) throw new HubException("코인이 부족합니다.");
                p.Coins -= cost;
                await db.SaveChangesAsync();
            }

            // 소유 목록
            var inv = await _shop.GetInventoryAsync(req.PlayerName);
            var ownedCos = new HashSet<string>(inv.OwnedCosmetics ?? Enumerable.Empty<string>(), StringComparer.OrdinalIgnoreCase);
            var ownedTh = new HashSet<string>(inv.OwnedThemes ?? Enumerable.Empty<string>(), StringComparer.OrdinalIgnoreCase);

            var results = new List<GachaResultItemDto>();
            var shardDelta = 0;

            for (int i = 0; i < count; i++)
            {
                var item = RollOne(banner.Pool);
                var isTheme = item.Kind.Equals("Theme", StringComparison.OrdinalIgnoreCase) || item.Id.StartsWith("theme_", StringComparison.OrdinalIgnoreCase);
                var already = isTheme ? ownedTh.Contains(item.Id) : ownedCos.Contains(item.Id);

                if (already)
                {
                    var dup = GachaBalance.ShardRefund[item.Rarity]; // 표준 테이블
                    _shards.AddOrUpdate(req.PlayerName, dup, (_, cur) => cur + dup);
                    shardDelta += dup;
                    results.Add(new GachaResultItemDto(item.Id, false, item.Rarity, dup));
                }
                else
                {
                    await GrantViaShopServiceAsync(req.PlayerName, item.Id, isTheme);
                    if (isTheme) ownedTh.Add(item.Id); else ownedCos.Add(item.Id);
                    results.Add(new GachaResultItemDto(item.Id, true, item.Rarity, 0));
                }
            }

            // 잔액/총 샤드 계산 후 반환
            await using var db2 = await _dbFactory.CreateDbContextAsync();
            var p2 = await db2.Players.FirstAsync(x => x.Name == req.PlayerName);
            var totalShards = _shards.TryGetValue(req.PlayerName, out var tot) ? tot : 0;

            return new GachaPullResult(cost, shardDelta, results, totalShards, p2.Coins);

            // 가중치 롤
            static GachaItemDto RollOne(List<GachaItemDto> pool)
            {
                int Weight(Rarity r) => r switch
                {
                    Rarity.Common => 100,
                    Rarity.Rare => 30,
                    Rarity.Epic => 10,
                    Rarity.Legendary => 2,
                    _ => 1
                };

                var total = pool.Sum(x => Weight(x.Rarity));
                var roll = _rng.Next(total);
                var acc = 0;
                foreach (var it in pool)
                {
                    acc += Weight(it.Rarity);
                    if (roll < acc) return it;
                }
                return pool[^1];
            }
        }

        public async Task<CraftResult> Craft(CraftRequest req)
        {
            int Cost(Rarity r) => r switch
            {
                Rarity.Common => 120,
                Rarity.Rare => 300,
                Rarity.Epic => 600,
                Rarity.Legendary => 900,
                _ => 300
            };

            EnsureDefaultBanners();
            var item = _gachaBanners.SelectMany(b => b.Pool)
                .FirstOrDefault(x => x.Id.Equals(req.ItemId, StringComparison.OrdinalIgnoreCase));
            if (item is null)
                return new(false, "제작 가능하지 않은 아이템입니다.", req.ItemId, 0,
                    _shards.TryGetValue(req.PlayerName, out var cur0) ? cur0 : 0);

            // ✅ 이미 보유한 경우 빠른 안내
            var invNow = await _shop.GetInventoryAsync(req.PlayerName);
            var isTheme = item.Id.StartsWith("theme_", StringComparison.OrdinalIgnoreCase) || item.Kind.Equals("Theme", StringComparison.OrdinalIgnoreCase);
            var alreadyOwned = isTheme
                ? invNow.OwnedThemes?.Contains(item.Id, StringComparer.OrdinalIgnoreCase) == true
                : invNow.OwnedCosmetics?.Contains(item.Id, StringComparer.OrdinalIgnoreCase) == true;
            if (alreadyOwned)
            {
                return new(false, "이미 보유한 아이템입니다.", req.ItemId, 0,
                    _shards.TryGetValue(req.PlayerName, out var curA) ? curA : 0);
            }

            _shards.TryGetValue(req.PlayerName, out var cur);
            var need = Cost(item.Rarity);
            if (cur < need) return new(false, "샤드가 부족합니다.", req.ItemId, 0, cur);

            // 샤드 선차감
            _shards[req.PlayerName] = cur - need;

            var ok = await GrantViaShopServiceAsync(req.PlayerName, item.Id, isTheme);
            if (!ok)
            {
                // 롤백
                _shards.AddOrUpdate(req.PlayerName, need, (_, v) => v + need);
                return new(false, "제작 실패(인벤토리 반영 실패)", req.ItemId, 0, _shards[req.PlayerName]);
            }

            return new(true, "제작 완료", req.ItemId, need, _shards[req.PlayerName]);
        }

        // ✅ GrantViaShopServiceAsync 보강(가격 확보 + 실패 1회 재시도)
        private async Task<bool> GrantViaShopServiceAsync(string playerName, string id, bool isTheme)
        {
            try
            {
                // 1) 가격 확보 (없으면 기본 1c)
                int price = 1;
                if (isTheme)
                {
                    var cat = await _shop.GetThemeCatalogAsync(playerName);
                    var found = cat.FirstOrDefault(x => x.Id.Equals(id, StringComparison.OrdinalIgnoreCase));
                    if (found != null) price = Math.Max(1, found.Price);
                }
                else
                {
                    var cat = await _shop.GetCosmeticCatalogAsync(playerName);
                    var found = cat.FirstOrDefault(x => x.Id.Equals(id, StringComparison.OrdinalIgnoreCase));
                    if (found != null) price = Math.Max(1, found.Price);
                }

                // 2) 임시 충전
                await using (var db = await _dbFactory.CreateDbContextAsync())
                {
                    var p = await db.Players.FirstAsync(x => x.Name == playerName);
                    p.Coins += price;
                    await db.SaveChangesAsync();
                }

                // 3) 구매 시도
                var res = isTheme
                    ? await _shop.BuyThemeAsync(playerName, id)
                    : await _shop.BuyCosmeticAsync(playerName, id);

                // 4) 환불
                await using (var db2 = await _dbFactory.CreateDbContextAsync())
                {
                    var p2 = await db2.Players.FirstAsync(x => x.Name == playerName);
                    p2.Coins -= price;
                    await db2.SaveChangesAsync();
                }

                // 5) 실패면 1회 재시도 (카탈로그 캐시/필터 흔들림 대비)
                if (!res.Ok)
                {
                    await using (var db3 = await _dbFactory.CreateDbContextAsync())
                    {
                        var p3 = await db3.Players.FirstAsync(x => x.Name == playerName);
                        p3.Coins += price;
                        await db3.SaveChangesAsync();
                    }

                    var res2 = isTheme
                        ? await _shop.BuyThemeAsync(playerName, id)
                        : await _shop.BuyCosmeticAsync(playerName, id);

                    await using (var db4 = await _dbFactory.CreateDbContextAsync())
                    {
                        var p4 = await db4.Players.FirstAsync(x => x.Name == playerName);
                        p4.Coins -= price;
                        await db4.SaveChangesAsync();
                    }

                    return res2.Ok;
                }

                return true;
            }
            catch
            {
                return false;
            }
        }


        // ── 외부 입력 DTO (null 허용)
        public record RegisterDto(
            string? LoginId, string? Password, string? PasswordConfirm,
            string? Name, string? Gender, string? Nick
        );

        // 룸 채팅 DTO
        public record RoomChatDto(string At, string RoomId, string Sender, string Text);


        public record RosterItem(string PlayerId, bool IsHost);


        // 전체 명단 + 호스트 플래그 전송
        private async Task BroadcastRoster(string roomId)
        {
            var host = _hostByRoom.TryGetValue(roomId, out var h) ? h : null;

            var items = _playerRoomById
                .Where(kv => kv.Value.Equals(roomId, StringComparison.OrdinalIgnoreCase))
                .Select(kv => new RosterItem(kv.Key, kv.Key == host))
                .ToArray();

            await Clients.Group(roomId).SendAsync("RoomRoster", items);
        }


    }
}
