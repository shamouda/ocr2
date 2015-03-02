from blist import sortedlist

class RECSortedList(sortedlist):
    def __init__(self, iterable=(), key=None, duplicateResolution=None):
        super(RECSortedList, self).__init__(iterable, key)
        self._duplicate = duplicateResolution

    def add(self, item, atEnd = False):
        """If atEnd, tries to append to the list (faster if correct)"""
        # We override this method to add duplicate resolution
        curLen = len(self)
        if curLen > 0:
            endItem = self[curLen-1]
        else:
            self._blist.append(self._u2i(item))
            return curLen

        if atEnd and self._u2key(endItem) <= self._u2key(item):
            if self._duplicate is not None and self._u2key(endItem) == self._u2key(item):
                if self._duplicate(endItem, item):
                    self[curLen-1] = item
                return curLen - 1
            else:
                self._blist.append(self._u2i(item))
                return curLen

        i, v = self._bisect_left(item)
        if self._duplicate is not None and v is not None and \
            self._u2key(item) == self._u2key(v):

            if self._duplicate(v, item):
                self[i] = item
        else:
            self._blist.insert(i, self._u2i(item))
        return i

    def find(self, value):
        """Returns a tuple (index, item) where item is the
        first item with a key that is the same as the key
        of value. Raise ValueError if not found."""
        try:
            i, v = self._bisect_left(value)
        except TypeError:
            raise ValueError
        if v is None or self._u2key(v) <> self._u2key(value):
            raise ValueError
        return (i, v)

    def value_in(self, value):
        """Returns True if the key 'k' where 'k' is
            value's key is present in the
            RECSortedList and False otherwise
        """
        try:
            self.find(value)
        except ValueError:
            return False
        else:
            return True

    def find_le(self, value):
        """Returns a tuple (index, item) where item is the
        *last* item with a key <= key(value). Raise ValueError
        if not found."""
        try:
            i, v = self._bisect_right(value)
        except TypeError:
            raise ValueError
        if i == 0:
            raise ValueError
        return (i-1, self[i-1])

    def find_lt(self, value):
        """Returns a tuple (index, item) where item is the
        *last* item with key < key(value). Raise ValueError
        if not found"""
        try:
            i, v = self._bisect_left(value)
        except TypeError:
            raise ValueError
        if i == 0:
            raise ValueError
        return (i-1, self[i-1])

    def find_ge(self, value):
        """Returns a tuple (index, item) where item is the
        *first* item with key >= key(value). Raise ValueError
        if not found"""
        try:
            i, v = self._bisect_left(value)
        except TypeError:
            raise ValueError
        if v is None:
            raise ValueError
        return (i, v)

    def find_gt(self, value):
        """Returns a tuple (index, item) where item is the
        *first* item with key > key(value). Raise ValueError
        if not found"""
        try:
            i, v = self._bisect_right(value)
        except TypeError:
            raise ValueError
        if v is None:
            raise ValueError
        return (i, v)

    def gen_range(self, value_low, value_high):
        """Returns a generator that generates (index, item) where item is
            such that key(item) >= key(value_low) and key(item) <= key(value_high)
        """
        try:
            low_index = self._bisect_left(value_low)[0] # like find_ge
            high_index = self._bisect_right(value_high)[0] # like find_le
            if high_index == 0:
                raise ValueError
            high_index -= 1
            for i in xrange(low_index, high_index+1):
                yield (i, self[i])
        except ValueError:
            pass

