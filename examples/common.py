def add_common_args(parser):
    parser.add_argument(
        "--fps",
        type=int,
        help="Desired frame rate for recording (default 30)",
        default=30,
    )
    parser.add_argument(
        "--x",
        type=int,
        help="Desired X origin of the mirror (default 0)",
        default=0,
    )
    parser.add_argument(
        "--y",
        type=int,
        help="Desired Y origin of the mirror (default 0)",
        default=0,
    )
    parser.add_argument(
        "--width",
        type=int,
        help="Desired width of the mirror (default 1024)",
        default=1024,
    )
    parser.add_argument(
        "--height",
        type=int,
        help="Desired height of the mirror (default 720)",
        default=720,
    )
    parser.add_argument(
        "--hwnd",
        help="Desired window handle to record.  You can find these using the Windows Kit tool named 'inspect'."
        + "Support hex format starting with 0x.",
    )
    parser.add_argument(
        "--point",
        help="Find window under the given point provided as 'x,y'",
    )
    parser.add_argument(
        "--process",
        help="Find window belonging to the given process id.",
    )
    parser.add_argument(
        "--debug",
        help="For debugging only",
        action="store_true",
    )
