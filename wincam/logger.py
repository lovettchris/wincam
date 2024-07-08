import logging

PREFIX = "wincam"

has_console_handler = False


class Logger:
    """Logger class to be used by all modules in the project.

    The available logging levels are given below in decreasing order of severity:
        CRITICAL
        ERROR
        WARNING
        INFO
        DEBUG

    When a log_level is set to a logger, logging messages which are less severe than it will be ignored.
    For example, if the log_level is set to INFO, then DEBUG messages will be ignored.
    """

    def __init__(self):
        self.root_logger = logging.getLogger(PREFIX)
        self.log_level = "INFO"

    def get_root_logger(self, log_level="INFO", log_file=None):
        self._setup(log_level, log_file)
        return self.root_logger

    def set_log_level(self, log_level):
        self.log_level = log_level
        self.root_logger.setLevel(log_level)
        for handler in self.root_logger.handlers:
            handler.setLevel(log_level)

    def set_log_file(self, log_file):
        self._add_file_handler(log_file)

    def _setup(self, log_level, log_file):
        self.log_level = log_level
        self.root_logger.setLevel(log_level)
        self._add_console_handler()
        if log_file:
            self._add_file_handler(log_file)

        # To avoid having duplicate logs in the console
        self.root_logger.propagate = False

    def _add_console_handler(self):
        global has_console_handler
        if not has_console_handler:
            console_handler_formatter = logging.Formatter("%(filename)s [%(levelname)s]: %(message)s")
            console_handler = logging.StreamHandler()
            console_handler.setLevel(self.log_level)
            console_handler.setFormatter(console_handler_formatter)
            self.root_logger.addHandler(console_handler)
            has_console_handler = True

    def _add_file_handler(self, log_file):
        file_handler_formatter = logging.Formatter(
            "%(asctime)s %(filename)s [%(levelname)s]: %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
        )
        file_handler = logging.FileHandler(log_file)
        file_handler.setLevel(self.log_level)
        file_handler.setFormatter(file_handler_formatter)
        self.root_logger.addHandler(file_handler)

    @staticmethod
    def get_logger(name):
        # We enforce the creation of a child logger (PREFIX.name) to keep the root logger setup
        if name.startswith(PREFIX + "."):
            return logging.getLogger(name)
        return logging.getLogger(PREFIX + "." + name)
