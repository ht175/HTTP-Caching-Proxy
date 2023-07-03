#include "proxy.hpp"
#include <cstdlib>


int main(void) {
    const char * port = "12345";
    proxy * a_proxy = new proxy(port);
    a_proxy->get_started();
    return EXIT_SUCCESS;
}