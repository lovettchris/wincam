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
        public void StartCapture(int w, int h);
        public ImageSource CaptureImage();
    }
}
