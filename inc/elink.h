#ifndef JOS_INC_LINK_H
#define JOS_INC_LINK_H

#include <inc/types.h>

struct EmbedLink {
    struct EmbedLink *prev, *next;
};

static inline void 
elink_init(struct EmbedLink *ln) 
{
    ln->prev = ln->next = ln;
}

static inline struct EmbedLink* 
elink_remove(struct EmbedLink *ln) 
{
    ln->prev->next = ln->next;
    ln->next->prev = ln->prev;
    elink_init(ln);
    return ln;
}

static inline void 
elink_insert(struct EmbedLink *pos, struct EmbedLink *ln) 
{
    ln->prev = pos, ln->next = pos->next;
    ln->prev->next = ln->next->prev = ln;
}

static inline bool 
elink_empty(struct EmbedLink * ln) 
{
    return ln->prev == ln && ln->next == ln;
}

static inline void 
elink_enqueue(struct EmbedLink *que, struct EmbedLink *ln) 
{
    elink_insert(que->prev, ln);
}

static inline struct EmbedLink*
elink_queue_head(struct EmbedLink* que)
{
    return que->next;
}

static inline struct EmbedLink* 
elink_dequeue(struct EmbedLink* que)
{
    return elink_remove(elink_queue_head(que));
}

#define offset(_t, _m) ((uint32_t)(&((_t*)0)->_m))
#define master(_x, _t, _m) ((_t*)((void*)(_x) - offset(_t, _m)))

#endif