using Microsoft.Extensions.Hosting;

namespace Snake.Server.Rooms;

public class GameLoopHostedService : BackgroundService
{
    private readonly RoomManager _rooms;
    private readonly int _tps;

    public GameLoopHostedService(RoomManager rooms, IConfiguration cfg)
    {
        _rooms = rooms;
        _tps = cfg.GetValue<int>("Game:TickRate", 12);
    }

    protected override async Task ExecuteAsync(CancellationToken ct)
    {
        var delay = TimeSpan.FromMilliseconds(1000.0 / _tps);
        while (!ct.IsCancellationRequested)
        {
            await _rooms.TickAllAsync();
            await Task.Delay(delay, ct);
        }
    }
}
