using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using Snake.Shared;

namespace Snake.Server.Core;

public class GameCore
{
   // === Drop pacing ===
   private int _tick;
   private int _coinsSpawned;
   private int _xpSpawned;
   private static class DropConfig
   {
       public const int CoinEveryTicks = 55;
       public const int XpEveryTicks = 75;
       public const int MaxCoins = 8;
       public const int MaxXp = 6;
   }

   private void SpawnCoin()
   {
           // TODO: 보드에 코인 스폰 로직 추가
   _coinsSpawned++;
       }

   private void SpawnXp()
   {
           // TODO: 보드에 XP 스폰 로직 추가
   _xpSpawned++;
       }


private readonly Dictionary<string, Player> _players = new();
    private Point _apple;
    private readonly BoardSize _size;

    public GameCore(BoardSize size)
    {
        _size = size;
        _apple = RandomApple();
        _tick = 0;
        _coinsSpawned = 0;
        _xpSpawned = 0;
    }

    public int Tick => _tick;
    public IReadOnlyCollection<Player> Players => _players.Values;

    public void AddPlayer(string pid, string name, int level, CosmeticInstance cosmetic)
    {
        var start = new Point(RandomX.Next(2, _size.W - 2), RandomX.Next(2, _size.H - 2));
        _players[pid] = new Player
        {
            Id = pid,
            Name = name,
            Level = level,    // ★ 저장
            Body = new List<Point> { start },
            Dir = Direction.Right,
            Alive = true,
            Score = 0,
            Cosmetic = cosmetic
        };
    }


    public void RemovePlayer(string pid) => _players.Remove(pid);

    public void ApplyInput(string pid, InputKey key)
    {
        if (!_players.TryGetValue(pid, out var p) || !p.Alive) return;
        var dir = key switch
        {
            InputKey.Up => Direction.Up,
            InputKey.Down => Direction.Down,
            InputKey.Left => Direction.Left,
            InputKey.Right => Direction.Right,
            _ => p.Dir
        };

        // 역방향 금지
        if ((p.Dir == Direction.Left && dir == Direction.Right) ||
            (p.Dir == Direction.Right && dir == Direction.Left) ||
            (p.Dir == Direction.Up && dir == Direction.Down) ||
            (p.Dir == Direction.Down && dir == Direction.Up))
            return;

        p.Dir = dir;
    }

    public void Update()
    {
        _tick++;
                // 주기적 드랍
        if (_tick % DropConfig.CoinEveryTicks == 0 && _coinsSpawned < DropConfig.MaxCoins)
           SpawnCoin();
        if (_tick % DropConfig.XpEveryTicks == 0 && _xpSpawned < DropConfig.MaxXp)
           SpawnXp();
        
        foreach (var p in _players.Values.Where(x => x.Alive))
        {
            var head = p.Body[0];
            var next = new Point(head.X + p.Dir.DX, head.Y + p.Dir.DY);

            if (!_size.InRange(next.X, next.Y) || Collides(next))
            {
                p.Alive = false;
                continue;
            }

            p.Body.Insert(0, next);

            if (next == _apple)
            {
                p.Score += 1;
                _apple = RandomApple();
            }
            else
            {
                // 꼬리 제거(성장 X)
                p.Body.RemoveAt(p.Body.Count - 1);
            }
        }
    }

    private bool Collides(Point next)
    {
        foreach (var pl in _players.Values)
            foreach (var b in pl.Body)
                if (b == next) return true;
        return false;
    }

    private Point RandomApple()
    {
        Point p;
        do
        {
            p = new Point(RandomX.Next(0, _size.W), RandomX.Next(0, _size.H));
        } while (_players.Values.Any(pl => pl.Body.Contains(p)));
        return p;
    }

    public Point Apple => _apple;   // ★ 추가


    public GameSnapshot MakeSnapshot(string roomId, MatchPhase phase, int phaseTicksRemaining)
    {
        var list = _players.Values.Select(p =>
            new PlayerStateDto(
                p.Id,
                p.Name,
                p.Level,             // ★ 추가
                p.Body.ToList(),
                p.Alive,
                p.Score,
                p.Cosmetic
            )).ToList();

        return new GameSnapshot(roomId, _tick, _apple, list, phase, phaseTicksRemaining);
    }


    // ====== 변경 포인트: record → mutable class ======
    public sealed class Player
    {
        public string Id { get; set; } = "";
        public string Name { get; set; } = "";
        public int Level { get; set; } = 1;   // ★ 추가
        public List<Point> Body { get; set; } = new();
        public Direction Dir { get; set; } = Direction.Right;
        public bool Alive { get; set; } = true;
        public int Score { get; set; } = 0;
        public CosmeticInstance Cosmetic { get; set; } =
            new CosmeticInstance("skin_basic_blue", "BASIC",
                new SnakeSkin("#2da8ff", "none", "none", "solid"));
    }


    public readonly record struct Direction(int DX, int DY)
    {
        public static Direction Up => new(0, -1);
        public static Direction Down => new(0, 1);
        public static Direction Left => new(-1, 0);
        public static Direction Right => new(1, 0);
    }
}
