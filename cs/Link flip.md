```c
#include <stdio.h>

struct list {
        int a;
        struct list *next;
};

void list_add(struct list *new, struct list *prev)
{
        prev->next = new;
}

void print_list(struct list *head)
{
        struct list *tmp = head;

        for (;tmp; tmp = tmp->next) {
                printf("%d\n", (*tmp).a);
        }

}

void list_flip(struct list *head)
{
        struct list *prev = NULL;
        struct list *tmp = head;
        struct list *next = NULL;

        for (;tmp;)  {
                next = tmp->next;

                tmp->next = prev;
                prev = tmp;

                tmp = next;
        }

        print_list(prev);
}

int main(int argc, char **argv)
{
        struct list list[6];

        for (int i = 0; i < 6; i++) {
                list[i].a = i;
                list_add(&list[i+1], &list[i]);
        }
        list[5].next = NULL;

        struct list *tmp = &list[0];

        print_list(tmp);

        list_flip(&list[0]);

        return 0;
}
```