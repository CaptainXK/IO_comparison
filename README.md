# IO_comparison
Comparsion of normal write, writev and aio write

Test buff is a char array in which each element is a 1M bytes char array. Write all elements to a single file and test the IO speeds of different IO tech.

# Usage

make && ./test.app
