using Microsoft.AspNetCore.SignalR.Client;
using Snake.Shared;
using Snake.Client.WinForms.Rendering;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace Snake.Client.WinForms;

public class MainForm : Form
{
    private readonly string _url, _name, _room;
    private HubConnection _conn = default!;
    private string _playerId = "";
    private GameSnapshot? _snap;

    // 🔽 Timer 모호성 제거: WinForms 타이머로 완전 수식
    private readonly System.Windows.Forms.Timer _render = new() { Interval = 16 };

    private int _cell = 16;

    public MainForm(string url, string name, string room)
    {
        _url = url; _name = name; _room = room;
        DoubleBuffered = true;
        Text = $"Snake - {_name}@{_room}";
        _render.Tick += (_, __) => Invalidate();
        _render.Start();
        KeyDown += OnKeyDown;

        Load += async (_, __) =>
        {
            _conn = new HubConnectionBuilder().WithUrl(_url).WithAutomaticReconnect().Build();
            _conn.On<GameSnapshot>("Snapshot", s => _snap = s);
            _conn.On<MatchFinishedDto>("MatchFinished", ShowRewards);

            await _conn.StartAsync();
            var accepted = await _conn.InvokeAsync<JoinAccepted>("Join", new JoinRequest(_name, _room));
            _playerId = accepted.PlayerId;
            Text += $"  | Lv.{accepted.AccountLevel} XP:{accepted.AccountXp}";
        };

        ClientSize = new Size(48 * _cell, 32 * _cell + 32);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.Clear(Color.FromArgb(16, 18, 24));

        if (_snap == null) return;

        e.Graphics.DrawString($"{_snap.Phase} - Tick:{_snap.ServerTick} Remain:{_snap.PhaseTicksRemaining}",
            Font, Brushes.White, 4, 4);

        var appleRect = CellRect(_snap.Apple);
        e.Graphics.FillEllipse(Brushes.Red, appleRect);

        foreach (var p in _snap.Players)
        {
            var isMe = p.PlayerId == _playerId;
            for (int i = 0; i < p.Body.Count; i++)
            {
                var r = CellRect(p.Body[i]);
                SkinPainter.PaintCell(e.Graphics, r, p.Cosmetic.Skin, head: i == 0);
                if (isMe && i == 0) e.Graphics.DrawRectangle(Pens.White, r);
            }

            // ★ 머리 위 라벨 (이름 + 레벨)
            var head = p.Body.Count > 0 ? p.Body[0] : new Point(0, 0);
            var pt = new Point(head.X * _cell, 32 + head.Y * _cell - 16);
            var label = $"{p.Name}  Lv.{p.Level}";
            e.Graphics.DrawString(label, Font, Brushes.White, pt);
        }

    }

    private Rectangle CellRect(Point pt) => new(pt.X * _cell, 32 + pt.Y * _cell, _cell, _cell);

    private async void OnKeyDown(object? sender, KeyEventArgs e)
    {
        if (string.IsNullOrEmpty(_playerId) || _conn.State != HubConnectionState.Connected) return;
        var key = e.KeyCode switch
        {
            Keys.Up => InputKey.Up,
            Keys.Down => InputKey.Down,
            Keys.Left => InputKey.Left,
            Keys.Right => InputKey.Right,
            _ => (InputKey)(-1)
        };
        if ((int)key >= 0)
            await _conn.InvokeAsync("SendInput",
                new InputCommand(_playerId, Environment.TickCount, key, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()));
    }

    private void ShowRewards(MatchFinishedDto r)
    {
        var me = r.Rewards.FirstOrDefault(x => x.PlayerId == _playerId);
        var text = string.Join(Environment.NewLine, r.Rewards
            .OrderBy(x => x.Rank)
            .Select(x => $"{x.Rank}. {x.Name}  +{x.GainedXp}XP  +{x.GainedCoins}c {(x.NewCosmeticUnlocked ? $"UNLOCK:{x.UnlockedCosmeticId}" : "")}"));
        MessageBox.Show(this, $"Match Finished!\n\n{text}\n\nYour Lv:{r.YourLevel} XP:{r.YourXp}", "Results");
    }
}
