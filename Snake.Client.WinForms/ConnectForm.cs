// Snake.Client.WinForms/ConnectForm.cs

using Snake.Client.WinForms;

public class ConnectForm : Form
{
    readonly TextBox txtUrl = new() { Text = "http://localhost:5000/hub", Dock = DockStyle.Top };
    readonly TextBox txtName = new() { Text = "Hyeonjun", Dock = DockStyle.Top };
    readonly TextBox txtRoom = new() { Text = "room-1", Dock = DockStyle.Top };
    readonly Button btn = new() { Text = "Connect", Dock = DockStyle.Top };

    public ConnectForm()
    {
        Text = "Snake Connect";
        Controls.Add(btn);
        Controls.Add(txtRoom);
        Controls.Add(txtName);
        Controls.Add(txtUrl);

        btn.Click += (_, __) =>
        {
            var f = new MainForm(txtUrl.Text, txtName.Text, txtRoom.Text);

            // ✅ 핵심: MainForm이 닫히면 ConnectForm도 닫아서 프로세스 종료
            f.FormClosed += (_, ____) => this.Close();

            this.Hide();
            f.Show();
        };
    }
}
