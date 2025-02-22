using System.IO;

namespace ScreenRecorder
{
    internal class FileHelpers
    {
        /// <summary>
        /// Search the PATH environment for a given program.
        /// </summary>
        /// <param name="programName"></param>
        /// <returns></returns>
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
