using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;

namespace WpfTestApp
{
    internal interface ICapture : IDisposable
    {
        public Task StartCapture(int x, int y, int timeout = 10000);

        public ImageSource CaptureImage();
    }
}
