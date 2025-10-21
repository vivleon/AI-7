using Snake.Server.Core;
using Snake.Shared;
using System.Drawing;

namespace Snake.Server.Services;

public class SnapshotMapper
{
    public GameSnapshot Build(string roomId, GameCore core, MatchPhase phase, int phaseTicksRemaining,
        IReadOnlyDictionary<string, (int Level, int Xp, string Name)> account)
    {
        var players = core.Players.Select(p =>
        {
            var lv = account.TryGetValue(p.Id, out var acc) ? acc.Level : 1;
            return new PlayerStateDto(p.Id, p.Name, lv, p.Body.ToList(), p.Alive, p.Score, p.Cosmetic);
        }).ToList();

        return new GameSnapshot(roomId, core.Tick, core.Apple, players, phase, phaseTicksRemaining);
    }
}
