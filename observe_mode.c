#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_list.h"

struct observe_s
{
    int id;
    char name[20];
    void (*notify)(void *data);
    struct list_head list;
};

struct server_s
{
    void (*notify)(struct server_s *server, void *data);
    void (*add_observe)(struct server_s *server, struct observe_s *obs);
    void (*del_observe)(struct server_s *server, int id);
    struct list_head head;
};


static void notify_observe_1(void *data)
{
    printf("notify_observe_1: %s\n", (char *)data);
}


static void notify_observe_2(void *data)
{
    printf("notify_observe_2: %s\n", (char *)data);
}


/**
 * @brief 通知所有观察者
 *
 * 遍历服务器中的所有观察者，并调用其通知回调函数，以通知所有观察者指定的数据。
 *
 * @param server 服务器指针
 * @param data 通知的数据
 */
static void notify_all(struct server_s *server, void *data)
{
    struct observe_s *obs;
    list_for_each_entry(obs, &server->head, list)
    {
        if (obs->notify != NULL)
        {
            obs->notify(data);
        }
    }
}


/**
 * @brief 添加观察对象
 *
 * 将观察对象添加到服务器的观察列表中。
 *
 * @param server 服务器结构体指针
 * @param obs 观察对象结构体指针
 */
static void add_observe(struct server_s *server, struct observe_s *obs)
{
    list_add(&obs->list, &server->head);
}


/**
 * @brief 删除观察对象
 *
 * 从给定的服务器中删除具有指定 ID 的观察对象。
 *
 * @param server 服务器指针
 * @param id 观察对象 ID
 */
static void del_observe(struct server_s *server, int id)
{
    struct observe_s *obs;
    list_for_each_entry(obs, &server->head, list)
    {
        if (obs->id == id)
        {
            list_del(&obs->list);
            break;
        }
    }
}


int main(void)
{
    // 分配内存给server结构体，并初始化
    struct server_s *server = (struct server_s *)calloc(1, sizeof(*server));
    if (server == NULL)
    {
        printf("calloc server failed\n");
        return -1;
    }

    INIT_LIST_HEAD(&server->head);
    server->notify = notify_all;
    server->add_observe = add_observe;
    server->del_observe = del_observe;

    struct observe_s *obs1 = (struct observe_s *)calloc(1, sizeof(*obs1));
    if (obs1 == NULL)
    {
        printf("calloc obs1 failed\n");
        return -1;
    }

    strcpy(obs1->name, "observe 1");
    obs1->id = 1;
    obs1->notify = notify_observe_1;
    // 将obs1添加到server的观察者列表中
    server->add_observe(server, obs1);

    struct observe_s *obs2 = (struct observe_s *)calloc(1, sizeof(*obs2));
    if (obs2 == NULL)
    {
        printf("calloc obs2 failed\n");
        return -1;
    }

    strcpy(obs2->name, "observe 2");
    obs2->id = 2;
    obs2->notify = notify_observe_2;
    // 将obs2添加到server的观察者列表中
    server->add_observe(server, obs2);

    printf("notify all\n");
    // 调用server的通知函数，通知所有观察者
    server->notify(server, "hello");

    // 从server的观察者列表中删除id为1的观察者
    server->del_observe(server, 1);

    printf("after del observe 1 notify all again\n");
    // 再次调用server的通知函数，通知所有观察者
    server->notify(server, "hello");

    free(obs1);
    free(obs2);
    free(server);

    return 0;
}