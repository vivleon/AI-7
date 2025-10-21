// Snake.Server/Data/AppDbContext.cs
using Microsoft.EntityFrameworkCore;

namespace Snake.Server.Data;

public class AppDbContext : DbContext
{
    public DbSet<Player> Players => Set<Player>();
    public DbSet<PlayerCosmetic> PlayerCosmetics => Set<PlayerCosmetic>();
    public DbSet<PlayerTheme> PlayerThemes => Set<PlayerTheme>();          // ★ 추가
    public DbSet<Match> Matches => Set<Match>();
    public DbSet<MatchPlayer> MatchPlayers => Set<MatchPlayer>();
    public DbSet<MovementLog> MovementLogs => Set<MovementLog>();

    // ★ 신규
    public DbSet<Account> Accounts => Set<Account>();

    public AppDbContext(DbContextOptions<AppDbContext> options) : base(options) { }

    protected override void OnModelCreating(ModelBuilder b)
    {
        base.OnModelCreating(b);

        b.Entity<PlayerCosmetic>()
            .HasIndex(x => new { x.PlayerName, x.CosmeticId })
            .IsUnique();

        // ★ PlayerTheme 고유키
        b.Entity<PlayerTheme>()
            .HasIndex(x => new { x.PlayerName, x.ThemeId })
            .IsUnique();

        // ★ Account 고유키
        b.Entity<Account>().HasIndex(a => a.LoginId).IsUnique();
        b.Entity<Account>().HasIndex(a => a.Nick).IsUnique();
    }
}
