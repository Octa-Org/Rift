#include <driver/driver.hh>

/// @brief The entry of RiftLang
/// @return 0 if the program exits successfully
int main(int argc, char **argv)
{
    rift::driver::Driver driver;
    driver.run(argc, argv);
    return 0;
}