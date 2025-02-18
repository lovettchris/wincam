using System.IO;

namespace WpfTestApp
{
    internal class FileHelpers
    {
        public static string FindProgramInPath(string programName)
        {
            if (File.Exists(programName))
            {
                return Path.GetFullPath(programName);
            }
            var paths = Environment.GetEnvironmentVariable("PATH").Split(Path.PathSeparator);
            foreach (var path in paths)
            {
                var fullPath = Path.Combine(path, programName);
                if (File.Exists(fullPath))
                {
                    return Path.GetFullPath(fullPath);
                }
            }
            return null;
        }
    }
}
