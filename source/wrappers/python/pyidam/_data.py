import abc


class Data():
    """
    The base class of data that can be returned by the pyidam Client.
    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def plot(self):
        pass

    @abc.abstractmethod
    def widget(self):
        pass
