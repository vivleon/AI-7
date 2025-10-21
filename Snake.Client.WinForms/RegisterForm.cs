//Snake.Client.WinForms/RegisterForm.cs

using System.Windows.Forms;

namespace Snake.Client.WinForms;

public class RegisterForm : Form
{
    public string LoginId => tbId.Text.Trim();
    public string Password => tbPw.Text;
    public string PasswordConfirm => tbPw2.Text;
    public string RealName => tbName.Text.Trim();
    public string Gender => (rbM.Checked ? "M" : rbF.Checked ? "F" : "");
    public string Nick => tbNick.Text.Trim();

    readonly TextBox tbId = new() { Dock = DockStyle.Top, PlaceholderText = "아이디" };
    readonly TextBox tbPw = new() { Dock = DockStyle.Top, PlaceholderText = "비밀번호", UseSystemPasswordChar = true };
    readonly TextBox tbPw2 = new() { Dock = DockStyle.Top, PlaceholderText = "비밀번호 확인", UseSystemPasswordChar = true };
    readonly TextBox tbName = new() { Dock = DockStyle.Top, PlaceholderText = "이름" };
    readonly TextBox tbNick = new() { Dock = DockStyle.Top, PlaceholderText = "닉네임" };
    readonly RadioButton rbM = new() { Dock = DockStyle.Top, Text = "남" };
    readonly RadioButton rbF = new() { Dock = DockStyle.Top, Text = "여" };
    readonly Button btnOk = new() { Dock = DockStyle.Bottom, Text = "가입" };
    readonly Button btnCancel = new() { Dock = DockStyle.Bottom, Text = "취소" };

    public RegisterForm()
    {
        Text = "회원가입"; Width = 320; Height = 360;
        Controls.Add(btnOk); Controls.Add(btnCancel);
        Controls.Add(rbF); Controls.Add(rbM);
        Controls.Add(tbNick);
        Controls.Add(tbName);
        Controls.Add(tbPw2);
        Controls.Add(tbPw);
        Controls.Add(tbId);

        btnOk.Click += (_, __) =>
        {
            if (Password != PasswordConfirm) { MessageBox.Show("비밀번호 확인이 일치하지 않습니다."); return; }
            if (LoginId.Length < 3 || Nick.Length < 2) { MessageBox.Show("아이디/닉네임 길이를 확인하세요."); return; }
            DialogResult = DialogResult.OK;
        };
        btnCancel.Click += (_, __) => DialogResult = DialogResult.Cancel;
    }
}
