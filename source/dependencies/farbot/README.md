# farbot

The farbot library was copied from https://github.com/hogliux/farbot (commit 914e21875ce6a92d15d3b8442f5ece94093f7d21, link: https://github.com/hogliux/farbot/commit/914e21875ce6a92d15d3b8442f5ece94093f7d21).

It was then slighltly changed:
- the file extensions were brought back to .h
- the .tcc files were included into the .h files
- the "noexcept" specifiers were removed, as they were not recognized during testing, and for the moment we do not need them
- the "std::atomic<T>::is_always_lock_free" is not available in C++11 nor C++12, so it was removed from a fifo assertion
- other small modifications