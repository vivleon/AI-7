namespace Snake.Server.Core;

public static class RandomX
{
    private static readonly ThreadLocal<Random> _rng =
        new(() => new Random(unchecked(Environment.TickCount * 31 + Thread.CurrentThread.ManagedThreadId)));

    public static int Next(int min, int max) => _rng.Value!.Next(min, max);
}
