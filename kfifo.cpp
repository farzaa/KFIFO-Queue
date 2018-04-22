#include <iostream>
#include <array>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>

// note - some of this code was inspired by the algorithms located at
// https://github.com/scala/scala
// nothing was directly taken.

using namespace std;
using namespace std::chrono;
struct Item {
    int value;
    int tag;

    Item(int value) {
        this->value = value;
        this->tag = 0;
    }
};


class KQueue {
    public:
    int size;
    int k;

    atomic<int> head;
    atomic<int> tail;

    // TODO Idea:
    // Instead of passing around the big ass array,
    // Can we just pass around the individual segemnts?
    // Not sure if that would help.
    // This is how they do it in the paper, after all.
    array<std::atomic<int>, 10000000> arr = {};

    std::atomic<int> jobs_completed;
    std::atomic<int> failed;

    KQueue(int size, int k) {
        this->size = size;
        this->k = k;

        this->jobs_completed = 0;
        this->failed = 0;

        head.store(0);
        tail.store(0);
    }

    int getSize(void) {
        return size;
    }

    bool is_queue_full(int head_old, int tail_old) {
        // the queue is full when the tail wraps around and meets the head.
        // we also want to make sure the head didn't change in the meantime
        // so a second check on head occurs here as well.
        if (((tail_old + k) % size) == head_old && (head_old == head.load())) {
                return true;
        }
        return false;
     }

    bool findIndex(int start, bool empty, int *item_index, int *old) {

        int i, index, random = rand() % k;
        for(i = 0; i < k; i++) {
            // First find a random index from [0 - k)
            index = (start + ((random + i) % k)) % size;
            // the number that is currently in this index.
            *old = arr[index].load();
            // We assume that the index is empty if the value is 0.
            // empty just specfies if we are looking for an empty spot, or a taken spot.
            if (((empty && *old == 0)) || (!empty && *old != 0)) {
                // both of these are pointers so that when they are changed
                // the changes can be seen in the orignal function.
                *item_index = index;
                return true;
            }
        }
        return false;
    }


     bool segment_has_stuff(int head_old) {
         int i, start = head_old;

         for(i = 0; i < k; i++) {
             if(arr[(start + i) % size].load() != 0) {
                 return true;
             }
         }
         return false;
     }

     bool in_valid_region(int tail_old, int tail_current, int head_current) {
         if(tail_current < head_current) {
             return true;
         } else {
             return false;
         }
     }


     bool not_in_valid_region(int tail_old, int tail_current, int head_current) {
         if(tail_current < head_current) {
             return true;
         } else {
             return false;
         }
     }


     bool committed(int tail_old, int item_new, int index) {
         if(arr[tail_old].load() != item_new) {
             return true;
         }

         int head_curr = head.load();
         int tail_curr = tail.load();

         if (in_valid_region(tail_old, tail_curr, head_curr)) {
             return true;
         } else if (not_in_valid_region(tail_old, tail_curr, head_curr)) {
           if (!arr[index].compare_exchange_strong(item_new, 0)) {
             return true;
           }
         } else {
           if (head.compare_exchange_strong(head_curr, head_curr)) {
             return true;
           }
           if (!arr[index].compare_exchange_strong(item_new, 0)) {
             return true;
           }
         }
         return false;

     }

    void move_tail_forward(int tail_old) {
        // TODO versioning
        tail.compare_exchange_strong(tail_old, (tail_old + k) % size);
    }

    void move_head_forward(int head_old) {
        // TODO versioning
        head.compare_exchange_strong(head_old, (head_old + k) % size);
    }

    bool enqueue(atomic<int> &new_item) {
        while(true) {
            int tail_old = tail.load();
            int head_old = head.load();

            int item_index, old;
            bool found_free_space = findIndex(tail_old, true, &item_index, &old);


            if (tail_old == tail.load()) {
                if (found_free_space) {
                    // TODO - implement version numbering. This would mean using atomic struct pointers.
                    // Not sure what the implications of this would be, need to think about it.
                    // printf("Got call to enqueue. Found free space at %d with value %d\n", item_index, old);

                    if (arr[item_index].compare_exchange_strong(old, new_item)) {
                        if (committed(tail_old, new_item, item_index)) {
                            return true;
                        }
                    }

                } else {
                    if (is_queue_full(head_old, tail_old)) {
                        // If our head segment has stuff, it means we are full.
                        if (segment_has_stuff(head_old) && head_old == head.load()) {
                            return false;
                        }

                        move_head_forward(head_old);

                    }

                    // check if queue is full AND the segemnt
                    move_tail_forward(tail_old);
                }
            }
        }
    }

    bool dequeue(int *item) {
        while (true) {

            int head_old = head.load();

            int item_index, old;
            bool found_index = findIndex(head_old, false, &item_index, &old);

            int tail_old = tail.load();

            if (head_old == head.load()) {
                // we found something!
                if (found_index) {
                    //  we don't want to be enqueing/dequeing from the same segment!
                    if (head_old == tail_old) {
                        move_tail_forward(tail_old);
                    }

                    if(arr[item_index].compare_exchange_strong(old, 0)) {
                        *item = old;
                        return true;
                    }
                } else {
                    if((head_old == tail_old) && (tail_old == tail.load())) {
                        return false;
                    }

                    move_head_forward(head_old);
                }
            }
        }
    }

    void printQueue() {
        int i;
        for(i = head.load(); i <= tail.load() + k - 1; i++) {
            if (i % k == 0) {
                printf(" - ");
            }
            printf("%d, ", arr[i].load());
        }
        printf("\n");
    }


    void do_work(int thread_number, atomic<int> items_to_add[], int length, bool deq, bool enq) {
        int i, dequeued_value;
        int count = 0;

        for(int i = 0; i < length; i++) {
            count += items_to_add[i];
        }


        for(i = 0; i < length; i++) {
            int randy = rand() % 2;
            if(randy == 0 && enq) {
                // printf("#%d    ---------------------enq(%d)----------------------- %d %d\n", thread_number, items_to_add[i].load(), head.load(), tail.load());
                bool s = enqueue(items_to_add[i]);
                this->jobs_completed++;
            }

            if(randy == 1 && deq) {
                // printf("#%d    ---------------------deq()----------------------- %d %d\n", thread_number, head.load(), tail.load());
                bool s = dequeue(&dequeued_value);
                this->jobs_completed++;
            }
        }
    }
};



int main()
{
    // Initialize an array of atomic integers to 0.
    int i, j;

    KQueue *qPointer = new KQueue(10000000, 32);
    int dequeued_value;
    int start_index = 0;

    // This is the only paramater that should be modified!
    int num_threads = 100;

    int total_jobs = 10000000;
    int jobs_per_thread = total_jobs/num_threads;
    int elems = 0;

    std::thread t[num_threads];
    // Create nodes before hand. This queue requires that
    // only unique items are added!
    atomic<int>** pre = new atomic<int>*[num_threads];
    for(i = 0; i < num_threads; i++) {
        pre[i] = new atomic<int>[jobs_per_thread];
        for(j = 0; j < jobs_per_thread; j++) {
            pre[i][j] = start_index + j;
            elems++;
        }
        start_index += jobs_per_thread;
    }


    for(i = 0; i < num_threads; i++) {
        t[i] = std::thread(&KQueue::do_work, qPointer, i, pre[i], jobs_per_thread, true, true);
    }

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    for(int i = 0; i < num_threads; i++) {
        t[i].join();
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>( t2 - t1 ).count();

    printf("\nTime elapsed in ms: %lld\n", duration);

    // qPointer->printQueue();

    return 0;
}
