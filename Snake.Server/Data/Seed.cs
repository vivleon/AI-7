// Snake.Server/Data/Seed.cs

namespace Snake.Server.Data;

public static class Seed
{
    public static async Task RunAsync(AppDbContext db)
    {
        if (!db.Players.Any())
        {
            db.Players.Add(new Player { Name = "Guest", Level = 1, Xp = 0, Coins = 0 });
            await db.SaveChangesAsync();
        }
    }
}
