namespace Snake.Server.Core;

public readonly record struct BoardSize(int W, int H)
{
    public bool InRange(int x, int y) => x >= 0 && y >= 0 && x < W && y < H;
}
