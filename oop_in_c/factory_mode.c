#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct base_s
{
    void (*operate)();
};

struct factory_s
{
    struct base_s *(*create)();
};

struct ob


void operate_1()
{
    printf("Operate 1\n");
}

void operate_2()
{
    printf("Operate 2\n");
}


struct base_s *create_obj_1()
{
    struct base_s *obj = (struct base_s *)calloc(1, sizeof(struct base_s));
    obj->operate = operate_1;
    return obj;
}



struct base_s *create_obj_2()
{
    struct base_s *obj = (struct base_s *)calloc(1, sizeof(struct base_s));
    obj->operate = operate_2;
    return obj;
}


int main(void)
{
    struct factory_s *factory = (struct factory_s *)calloc(1, sizeof(struct factory_s));
    factory->create = create_obj_1;

    struct base_s *obj = NULL;
    obj = factory->create();
    if (obj)
    {
        obj->operate();
        free(obj);
    }

    factory->create = create_obj_2;
    obj = factory->create();
    if (obj)
    {
        obj->operate();
        free(obj);
    }

    free(factory);
    
    return 0;
}


