# 单链表原地反转

> 逐个反转 `next` 指针，并返回新的头节点；调用方必须保存返回值。

## 实现

```c
#include <stddef.h>

struct list {
    int a;
    struct list *next;
};

struct list *list_flip(struct list *head)
{
    struct list *prev = NULL;

    while (head != NULL) {
        struct list *next = head->next;
        head->next = prev;
        prev = head;
        head = next;
    }
    return prev;
}
```

## 使用与边界

- 调用 `head = list_flip(head);`，否则调用方仍指向原头节点。
- 空链表返回 `NULL`；单节点链表保持不变。
- 时间复杂度为 O(n)，除少量指针外不分配额外空间。
