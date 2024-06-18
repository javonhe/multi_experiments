#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

typedef struct section_s
{
    char *name;
    void (*func)(struct section_s *);
} s_section_t;


extern s_section_t _mysec_start[];
extern s_section_t _mysec_end[];

static void func_sec(s_section_t *section)
{
    printf("section: %s\n", section->name);
}

s_section_t section_1 __attribute__((unused, section(".mysec"))) = { "section_1_s", func_sec};
s_section_t section_2 __attribute__((unused, section(".mysec"))) = { "section_2_s", func_sec};
s_section_t section_3 __attribute__((unused, section(".mysec"))) = { "section_3_s", func_sec};


int main(void)
{
    s_section_t *p = _mysec_start;
    while (p < _mysec_end)
    {
        if (p->func != NULL)
            p->func(p);
        p++;
    }

    return 0;
}
