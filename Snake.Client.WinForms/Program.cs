// Snake.Client.WinForms/Program.cs
using System;
using System.Windows.Forms;

namespace Snake.Client.WinForms;
internal static class Program
{
    [STAThread]
    static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new UnifiedMainForm());  // ¡Ú ´ÜÀÏ Æû
    }
}
