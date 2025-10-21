// Snake.Server/Data/DbBootstrap.cs
using Microsoft.Data.Sqlite;
using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Snake.Server.Data
{
    public static class DbBootstrap
    {
        public static async Task EnsureUpToDateAsync(AppDbContext db)
        {
            var conn = (SqliteConnection)db.Database.GetDbConnection();
            bool needRecreate = false;

            await conn.OpenAsync();
            try
            {
                bool TableExists(string name)
                {
                    using var cmd = conn.CreateCommand();
                    cmd.CommandText = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=$name LIMIT 1";
                    cmd.Parameters.AddWithValue("$name", name);
                    using var r = cmd.ExecuteReader();
                    return r.Read();
                }

                HashSet<string> GetPlayerCols()
                {
                    using var cmd = conn.CreateCommand();
                    cmd.CommandText = "PRAGMA table_info(Players)";
                    using var r = cmd.ExecuteReader();
                    var cols = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                    while (r.Read()) cols.Add(r.GetString(1));
                    return cols;
                }

                // ★ Accounts, PlayerThemes 포함
                var requiredTables = new[] { "Players", "PlayerCosmetics", "PlayerThemes", "Accounts" };
                var requiredPlayerColumns = new[]
                {
                    "Id","Name","Level","Xp","Coins","SelectedCosmeticId","SelectedThemeId","EmojiTag"
                };

                var okTables = requiredTables.All(TableExists);
                var okColumns = false;

                if (okTables)
                {
                    var cols = GetPlayerCols();
                    okColumns = requiredPlayerColumns.All(cols.Contains);
                }

                needRecreate = !okTables || !okColumns;
            }
            finally
            {
                // 삭제/생성 전에 반드시 닫기
                await conn.CloseAsync();
            }

            if (needRecreate)
            {
                Console.WriteLine("[DB] Schema mismatch detected. Recreating sqlite database...");
                await db.Database.EnsureDeletedAsync();
                await db.Database.EnsureCreatedAsync();
                Console.WriteLine("[DB] Recreated.");
            }
        }
    }
}
