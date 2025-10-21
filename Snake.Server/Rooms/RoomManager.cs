//Snake.Server/Rooms/RoomManager.cs
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;
using Snake.Server.Core;
using Snake.Server.Rooms;
using Snake.Shared;
using Microsoft.AspNetCore.SignalR;

namespace Snake.Server.Rooms;

public class RoomManager
{
    private readonly ConcurrentDictionary<string, GameRoom> _rooms = new();
    private readonly IServiceProvider _sp;

    public RoomManager(IServiceProvider sp) => _sp = sp;

    public async Task<JoinAccepted> JoinAsync(string connId, string remoteIp, JoinRequest req)
    {
        var room = _rooms.GetOrAdd(req.RoomId, id =>
            ActivatorUtilities.CreateInstance<GameRoom>(_sp, id));

        return await room.JoinAsync(connId, remoteIp, req);
    }

    // 입력 큐
    public Task EnqueueInputAsync(InputCommand cmd)
    {
        var roomId = ExtractRoomId(cmd.PlayerId);
        if (_rooms.TryGetValue(roomId, out var room))
            return room.EnqueueInput(cmd);
        return Task.CompletedTask;
    }

    // 준비 토글
    public Task SetReadyAsync(string playerId, bool ready)
    {
        var roomId = ExtractRoomId(playerId);
        if (_rooms.TryGetValue(roomId, out var room))
            room.SetReady(playerId, ready);
        return Task.CompletedTask;
    }

    // 연결 단위 방 나가기
    public Task LeaveByConnectionAsync(string connId, string reason)
    {
        foreach (var kv in _rooms.ToArray())
        {
            kv.Value.LeaveByConnection(connId, reason);
            // 방이 비었으면 정리
            if (kv.Value.PlayerCount == 0)
            {
                _rooms.TryRemove(kv.Key, out _);
            }
        }
        return Task.CompletedTask;
    }

    // 전체 틱
    public async Task TickAllAsync()
    {
        foreach (var room in _rooms.Values)
            await room.TickAsync();

        // 주기적 청소(유령방 제거)
        foreach (var kv in _rooms.ToArray())
            if (kv.Value.PlayerCount == 0)
                _rooms.TryRemove(kv.Key, out _);
    }

    // 방 리스트
    public Task<List<RoomBrief>> ListRoomsAsync()
    {
        var list = _rooms.Values.Select(r => r.Brief()).ToList();
        return Task.FromResult(list);
    }

    // === 방장/시작 래퍼(허브 리플렉션에서 찾는 이름과 일치) ===
    public Task<bool> AmIHostAsync(string playerId)
    {
        var roomId = ExtractRoomId(playerId);
        if (_rooms.TryGetValue(roomId, out var room))
            return Task.FromResult(room.IsHost(playerId));
        return Task.FromResult(false);
    }

    public Task<bool> StartMatchAsync(string playerId)
    {
        var roomId = ExtractRoomId(playerId);
        if (_rooms.TryGetValue(roomId, out var room))
            return room.TryStartByHostAsync(playerId);
        return Task.FromResult(false);
    }
    public bool ApplyCosmeticForPlayer(string playerName)
    {
        // 어떤 방에 있는지 찾아서 반영
        foreach (var r in _rooms.Values)
        {
            // r 내부에 플레이어 이름이 있는지 검사
            // (간단화를 위해 GameRoom에 helper 하나 더 만들어도 OK)
        }
        return false;
    }

    // RoomManager.cs 내부
    public bool ApplyCosmeticForName(string roomId, string playerName, string cosmeticId)
    {
        if (_rooms.TryGetValue(roomId, out var room))
            return room.ApplyCosmeticForName(playerName, cosmeticId);
        return false;
    }

    // 필요하면 재사용할 수 있도록 TryGet 유틸도 넣어둡니다(선택).
    public bool TryGet(string roomId, out GameRoom? room)
        => _rooms.TryGetValue(roomId, out room);

    //위임

    public Task<bool> DelegateHostAsync(string roomId, string hostPlayerId, string targetName)
    {
        if (_rooms.TryGetValue(roomId, out var room))
            return Task.FromResult(room.DelegateHost(hostPlayerId, targetName));
        return Task.FromResult(false);
    }


    // 강퇴
    public Task<bool> KickPlayerAsync(string roomId, string hostPlayerId, string targetName)
    {
        if (_rooms.TryGetValue(roomId, out var room))
            return Task.FromResult(room.KickByName(hostPlayerId, targetName));
        return Task.FromResult(false);
    }

    public Task<bool> SkipRemainAsync(string roomId, string hostPlayerId)
    {
        if (_rooms.TryGetValue(roomId, out var room))
            return Task.FromResult(room.SkipRemain(hostPlayerId));
        return Task.FromResult(false);
    }

    // 방 존재/비었는지 확인 (허브에서 참조 가능)
    public bool IsRoomEmpty(string roomId)
        => _rooms.TryGetValue(roomId, out var r) ? r.PlayerCount == 0 : true;

    public IEnumerable<string> CurrentRoomIds() => _rooms.Keys;

    private static string ExtractRoomId(string playerId)
    {
        // playerId 형식: "{roomId}:{guid}"
        var i = playerId.LastIndexOf(':');
        return i > 0 ? playerId[..i] : "default";
    }
}
