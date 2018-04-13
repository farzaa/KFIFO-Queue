#include <iostream>
#include <array>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>

using namespace std;

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
    int k = 2;


    atomic<int> head;
    atomic<int> tail;
    array<std::atomic<int>, 100> arr = {};
    array<std::atomic<Item*>, 100> a = {};

    KQueue(int size) {
        this->size = size;
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
         int i = 0;
         int start = head_old;

         for(i = 0; i < k; i++) {
             if(arr[(start + i) % size].load() != 0) {
                 return true;
             }
         }

         return false;
     }

     // The in_valid_region and not_in_valid_region functions
     // were written with the assistance of the scal library.
     // https://github.com/cksystemsgroup/scal
     bool in_valid_region(int tail_old, int tail_current, int head_current) {
         bool wrap_around = (tail_current < head_current) ? true : false;
         if (!wrap_around) {
           return (head_current < tail_old && tail_old <= tail_current) ? true : false;
       }

         return (head_current < tail_old || tail_old <= tail_current) ? true : false;
         return true;
     }


     bool not_in_valid_region(int tail_old, int tail_current, int head_current) {
         bool wrap_around = (tail_current < head_current)
                            ? true : false;
         if (!wrap_around) {
           return (tail_old < tail_current
                   || head_current < tail_old) ? true : false;
         }
         return (tail_old < tail_current
                 && head_current < tail_old) ? true : false;
         return true;
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
             // TODO versioning!
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

    bool enqueue(int item_to_add) {
        atomic<int> new_item = ATOMIC_VAR_INIT(item_to_add);
        while(true) {

            Item i (item_to_add);

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
                        return true;
                    }

                } else {
                    if (is_queue_full(head_old, tail_old)) {
                        // If our head segment has stuff, it means we are full.
                        if (segment_has_stuff(head_old) && head_old == head.load()) {
                            return false;
                        }
                        // TODO: this is in the paper but not sure why! (???)
                        // If the head didn't have stuff, we just increment head.
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
        for(i = head.load(); i <= tail.load() + 1; i++) {
            if (i % k == 0) {
                printf(" - ");
            }
            printf("%d, ", arr[i].load());
        }
        printf("\n");
    }

    void do_work(int thread_number, int items_to_add[], int length) {
        int i, dequeued_value;

        for(i = 0; i < length; i++) {
            int randy = rand() % 2;
            if(randy == 0) {
                // printf("#%d    ---------------------enq(%d)-----------------------\n", thread_number, items_to_add[i]);
                bool s = enqueue(items_to_add[i]);
            } else {
                // printf("#%d    ---------------------deq()-----------------------\n", thread_number);
                bool s = dequeue(&dequeued_value);
            }

        }
    }
};




int main()
{
    // Initialize an array of atomic integers to 0.
    int i, j;

    KQueue *qPointer = new KQueue(10000);
    int dequeued_value;



    int start_index = 1;


    int num_threads = 3;
    int total_jobs = 8000;


    int jobs_per_thread = total_jobs/num_threads;

    std::thread t[num_threads];

    int** all_jobs = new int*[num_threads];

    for(i = 0; i < num_threads; i++) {
        all_jobs[i] = new int[jobs_per_thread];
        for(j = 0; j < jobs_per_thread; j++) {
            all_jobs[i][j] = start_index + j;
        }
        start_index += jobs_per_thread;
    }

    // for(int i = 0; i < num_threads; i++) {
    //     for(int j = 0; j < jobs_per_thread; j++) {
    //         printf(" %d ", all_jobs[i][j]);
    //     }
    // }

    clock_t start = clock();

    for(i = 0; i < num_threads; i++) {
        t[i] = std::thread(&KQueue::do_work, qPointer, i, all_jobs[i], jobs_per_thread);
    }

    for(int i = 0; i < num_threads; ++i) {
        t[i].join();
    }

    clock_t stop = clock();
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Time elapsed in ms: %f\n", elapsed);

    qPointer->printQueue();

    return 0;
}
