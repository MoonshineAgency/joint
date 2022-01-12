#include "test.h"
#include "system.h"

#include "iot.h"

static iot::numeric_source<float> *src;

void test_run()
{
    src = new iot::numeric_source<float>(SYSTEM_ID, "test", 10);
    src->set_mode(iot::MODE_NETWORK);

    for (size_t i = 0; i < 100; i++)
        src->send(i / 3.0f);
}
