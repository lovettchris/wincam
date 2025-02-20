namespace WpfTestApp;
using System.Windows.Threading;

/// <summary>
/// A simple helper class that gives a way to run things on the UI thread.  The app must call Initialize once during app start, using inside OnLaunch.
/// </summary>
public class UiDispatcher
{
    static UiDispatcher instance;
    Dispatcher dispatcher;

    public static void Initialize()
    {
        instance = new UiDispatcher()
        {
            dispatcher = Dispatcher.CurrentDispatcher
        };
    }

    public static Task RunOnUIThreadAsync(Action a)
    {
        return instance.dispatcher.InvokeAsync(a).Task;
    }
}
