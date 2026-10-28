#include <assert.h>
#include <stdio.h>
#include "../../include/battle/tag_clash_sequence.h"
int main(void) {
    TagClashSequence a,b,snapshot; int i,button;
    TagSequence_Init(&a,123); TagSequence_Init(&b,123);
    for(i=0;i<50;i++) {
        button=a.button;
        assert(TagSequence_Press(&a,i,1u<<button));
        assert(TagSequence_Press(&b,i,1u<<button));
        assert(a.button==b.button && a.button!=button && a.score==i+1);
        assert(TagSequence_Press(&a,i,15)); /* cached query, not a second score */
        assert(a.score==i+1);
    }
    snapshot=a;
    assert(!TagSequence_Press(&a,50,15));
    for(i=51;i<62;i++) assert(!TagSequence_Press(&a,i,1u<<a.button));
    assert(a.score==50 && a.button==snapshot.button);
    assert(TagSequence_Press(&a,62,1u<<a.button));
    button=a.button;
    a=snapshot; /* rollback followed by the same inputs */
    TagSequence_Press(&a,50,15);
    for(i=51;i<62;i++) TagSequence_Press(&a,i,1u<<a.button);
    TagSequence_Press(&a,62,1u<<a.button);
    assert(a.score==51 && a.button==button);
    assert(!TagSequence_Press(&a,63,0)); /* held button without another edge */
    assert(b.score==50); /* another pair/seat has independent progress */
    puts("PASS sequence edges, random prompts, mistake penalty, independent seats and rollback");
}
