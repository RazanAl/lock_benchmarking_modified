#include <stdlib.h>

#include "dr_multi_writers_queue.h"
#include "smp_utils.h"

inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}

DRMWQueue * drmvqueue_create(){
    DRMWQueue * queue = malloc(sizeof(DRMWQueue));
    return drmvqueue_initialize(queue);
}

DRMWQueue * drmvqueue_initialize(DRMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i].request = NULL;
        queue->elements[i].data = NULL;
        queue->elements[i].responseLocation = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void drmvqueue_free(DRMWQueue * queue){
    free(queue);
}

inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index].data, e.data);
            store_rel(queue->elements[index].request, e.request);
            __sync_synchronize();//Flush
            return true;
        }else{
            store_rel(queue->closed.value, true);
            __sync_synchronize();//Flush
            return false;
        }
    }else{
        return false;
    }
}

inline
void drmvqueue_flush(DRMWQueue * queue){
    unsigned long numOfElementsToRead;
    unsigned long newNumOfElementsToRead;
    unsigned long currentElementIndex = 0;
    bool closed = false;
    load_acq(numOfElementsToRead, queue->elementCount.value);
    if(numOfElementsToRead >= MWQ_CAPACITY){
        closed = true;
        numOfElementsToRead = MWQ_CAPACITY;
    }

    while(true){
        if(currentElementIndex < numOfElementsToRead){
            //There is definitly an element that we should read
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[currentElementIndex].request);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[currentElementIndex].request);
            }
            load_acq(e.data, queue->elements[currentElementIndex].data);
            load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[currentElementIndex].request, NULL);
            currentElementIndex = currentElementIndex + 1;
        }else if (closed){
            //The queue is closed and there is no more elements that need to be read:
            return;
        }else{
            //Seems like there are no elements that should be read and the queue is
            //not closed. Check again if there are still no more elements that should
            //be read before closing the queue
            load_acq(newNumOfElementsToRead, queue->elementCount.value);
            if(newNumOfElementsToRead == numOfElementsToRead){
                //numOfElementsToRead has not changed. Close the queue.
                numOfElementsToRead = 
                    min(get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY + 1), 
                        MWQ_CAPACITY);
                closed = true;
            }else if(newNumOfElementsToRead < MWQ_CAPACITY){
                numOfElementsToRead = newNumOfElementsToRead;
            }else{
                closed = true;
                numOfElementsToRead = MWQ_CAPACITY;
            }
        }
    }
}

inline
void drmvqueue_reset_fully_read(DRMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}
