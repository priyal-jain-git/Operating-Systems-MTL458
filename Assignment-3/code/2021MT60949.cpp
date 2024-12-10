#include <iostream>
#include <unordered_map>
#include <cstdint>

using namespace std;

// Node class representing each element in a the MyDS
template<typename T>
class Node {
public:
    T data;  // Holds the value of the node
    Node* next;  // Pointer to the next node
    Node* prev;  // Pointer to the previous node

    // Constructor to initialize node with data and null pointers
    Node(T value) : data(value), next(nullptr), prev(nullptr) {}
};

// MyDS class acts as a data structure that can work as a queue, stack, or doubly linked list
template<typename T>
class MyDS {
private:
    Node<T>* head;  // Points to the front of the structure
    Node<T>* tail;  // Points to the back of the structure
    int size;  // Keeps track of the number of elements

public:
    // Constructor initializes an empty list
    MyDS() : head(nullptr), tail(nullptr), size(0) {}

    // Destructor cleans up all nodes to avoid memory leaks
    ~MyDS() {
        while (!isEmpty()) {
            popFront();  // Keep removing from the front until the list is empty
        }
    }

    // Function to push a value to the front of the list (used in LRU and Stack)
    void pushFront(T value) {
        Node<T>* newNode = new Node<T>(value);  // Create a new node with the value
        if (isEmpty()) {  // If the list is empty, both head and tail point to the new node
            head = tail = newNode;
        } else {
            newNode->next = head;  // New node points to the current head
            head->prev = newNode;  // The old head's previous points to the new node
            head = newNode;  // Update head to the new node
        }
        size++;  // Increment the size of the list
    }

    // Function to push a value to the back of the list (used in FIFO and Optimal)
    void pushBack(T value) {
        Node<T>* newNode = new Node<T>(value);  // Create a new node with the value
        if (isEmpty()) {  // If the list is empty, both head and tail point to the new node
            head = tail = newNode;
        } else {
            newNode->prev = tail;  // New node's previous points to the current tail
            tail->next = newNode;  // The current tail's next points to the new node
            tail = newNode;  // Update tail to the new node
        }
        size++;  // Increment the size of the list
    }

    // Function to pop the front element from the list (used in FIFO, LRU)
    void popFront() {
        if (isEmpty()) return;  // If the list is empty, do nothing
        Node<T>* temp = head;  // Temporarily store the current head
        head = head->next;  // Move head to the next node
        if (head) head->prev = nullptr;  // If there's a new head, set its previous to null
        else tail = nullptr;  // If no nodes remain, set tail to null
        delete temp;  // Delete the old head
        size--;  // Decrease the size of the list
    }

    // Function to pop the back element from the list (used in LIFO and LRU)
    void popBack() {
        if (isEmpty()) return;  // If the list is empty, do nothing
        Node<T>* temp = tail;  // Temporarily store the current tail
        tail = tail->prev;  // Move tail to the previous node
        if (tail) tail->next = nullptr;  // If there's a new tail, set its next to null
        else head = nullptr;  // If no nodes remain, set head to null
        delete temp;  // Delete the old tail
        size--;  // Decrease the size of the list
    }

    // Function to remove a specific node (used in LRU)
    void removeNode(Node<T>* node) {
        if (!node) return;  // If the node is null, do nothing
        if (node->prev) node->prev->next = node->next;  // Update previous node's next
        else head = node->next;  // If node is head, move head to next node
        if (node->next) node->next->prev = node->prev;  // Update next node's previous
        else tail = node->prev;  // If node is tail, move tail to previous node
        delete node;  // Delete the node
        size--;  // Decrease the size of the list
    }

    // Function to get the front element (used in FIFO and LRU)
    T getFront() {
        if (isEmpty()) throw std::runtime_error("Structure is empty");  // Throw error if empty
        return head->data;  // Return the data of the head node
    }

    // Function to get the back element (used in LIFO and LRU)
    T getBack() {
        if (isEmpty()) throw std::runtime_error("Structure is empty");  // Throw error if empty
        return tail->data;  // Return the data of the tail node
    }

    // Function to check if the structure is empty
    bool isEmpty() {
        return head == nullptr;  // True if head is null (list is empty)
    }

    // Function to return the current size of the structure
    int getSize() {
        return size;  // Return the size of the list
    }

    // Iterator class for traversal of MyDS elements
    class Iterator {
    private:
        Node<T>* current;  // Points to the current node
    public:
        // Constructor to initialize the iterator with a node
        Iterator(Node<T>* node) : current(node) {}

        // Dereference operator to access node's data
        T& operator*() {
            return current->data;
        }

        // Prefix increment operator to move to the next node
        Iterator& operator++() {
            if (current) current = current->next;  // Move to the next node
            return *this;
        }

        // Not-equal operator to compare two iterators
        bool operator!=(const Iterator& other) const {
            return current != other.current;  // True if iterators point to different nodes
        }

        // Get the node currently pointed to by the iterator
        Node<T>* getNode() const {
            return current;
        }
    };

    // Function to get an iterator pointing to the head (beginning) of the structure
    Iterator begin() const { return Iterator(head); }

    // Function to get an iterator pointing to the end (nullptr) of the structure
    Iterator end() const { return Iterator(nullptr); }
};

// TLB Simulator class that uses MyDS to simulate various page replacement strategies
class TLBSimulator {
private:
    int tlbSize;  // TLB capacity (maximum number of pages it can hold)

    // Function to extract the page number from an address, given a page size
    uint32_t getPageNumber(uint32_t address, uint32_t pageSize) {
        return address / (pageSize * 1024);  // Divide by page size in KB to get the page number
    }

    // Function to check if a page is already in the TLB
    bool isPageInTLB(const unordered_map<uint32_t, bool>& tlbEntries, uint32_t page) {
        return tlbEntries.find(page) != tlbEntries.end();  // Check if the page is in the TLB
    }

    // ** FIFO Simulation Functions **

    // Function to handle a TLB hit in FIFO strategy
    void handleTLBHitFIFO(int& hits) {
        hits++;  // Increment hit count
    }

    // Function to handle a TLB miss in FIFO strategy
    void handleTLBMissFIFO(unordered_map<uint32_t, bool>& tlbEntries,
                           MyDS<uint32_t>& fifoQueue,
                           uint32_t page) {
        if (fifoQueue.getSize() == tlbSize) {  // If TLB is full
            uint32_t frontPage = fifoQueue.getFront();  // Get the page at the front (oldest page)
            tlbEntries.erase(frontPage);  // Remove the oldest page from the TLB entries
            fifoQueue.popFront();  // Remove the front page from the FIFO queue
        }
        fifoQueue.pushBack(page);  // Add the new page to the back of the FIFO queue
        tlbEntries[page] = true;  // Mark the new page as being in the TLB
    }

    // Function to simulate FIFO page replacement strategy
    int simulateFIFO(const uint32_t* pageNumbers, int numAccesses) {
        unordered_map<uint32_t, bool> tlbEntries;  // TLB entries map
        MyDS<uint32_t> fifoQueue;  // FIFO queue
        int hits = 0;  // Count of TLB hits

        for (int i = 0; i < numAccesses; i++) {
            uint32_t page = pageNumbers[i];  // Get the current page

            if (isPageInTLB(tlbEntries, page)) {
                handleTLBHitFIFO(hits);  // If page is in TLB, it's a hit
            } else {
                handleTLBMissFIFO(tlbEntries, fifoQueue, page);  // Otherwise, it's a miss
            }
        }
        return hits;  // Return the total number of hits
    }

    // ** LIFO Simulation Functions **

    // Function to handle a TLB hit in LIFO strategy
    void handleTLBHitLIFO(int& hits) {
        hits++;  // Increment hit count
    }

    // Function to handle a TLB miss in LIFO strategy
    void handleTLBMissLIFO(unordered_map<uint32_t, bool>& tlbEntries,
                           MyDS<uint32_t>& lifoStack,
                           uint32_t page) {
        if (lifoStack.getSize() == tlbSize) {  // If TLB is full
            uint32_t backPage = lifoStack.getBack();  // Get the page at the back (most recent page)
            tlbEntries.erase(backPage);  // Remove the most recent page from the TLB entries
            lifoStack.popBack();  // Remove the back page from the LIFO stack
        }
        lifoStack.pushBack(page);  // Add the new page to the back of the LIFO stack
        tlbEntries[page] = true;  // Mark the new page as being in the TLB
    }

    // Function to simulate LIFO page replacement strategy
    int simulateLIFO(const uint32_t* pageNumbers, int numAccesses) {
        unordered_map<uint32_t, bool> tlbEntries;  // TLB entries map
        MyDS<uint32_t> lifoStack;  // LIFO stack
        int hits = 0;  // Count of TLB hits

        for (int i = 0; i < numAccesses; i++) {
            uint32_t page = pageNumbers[i];  // Get the current page

            if (isPageInTLB(tlbEntries, page)) {
                handleTLBHitLIFO(hits);  // If page is in TLB, it's a hit
            } else {
                handleTLBMissLIFO(tlbEntries, lifoStack, page);  // Otherwise, it's a miss
            }
        }
        return hits;  // Return the total number of hits
    }

    // ** LRU Simulation Functions **

    // Function to handle a TLB hit in LRU strategy
    void handleTLBHitLRU(int& hits,
                         unordered_map<uint32_t, Node<uint32_t>*>& cache,
                         MyDS<uint32_t>& lruList,
                         uint32_t page) {
        hits++;  // Increment hit count
        lruList.removeNode(cache[page]);  // Remove the page from its current position in the LRU list
        lruList.pushFront(page);  // Move the page to the front of the LRU list
        cache[page] = lruList.begin().getNode();  // Update the cache with the new position
    }

    // Function to handle a TLB miss in LRU strategy
    void handleTLBMissLRU(unordered_map<uint32_t, Node<uint32_t>*>& cache,
                          MyDS<uint32_t>& lruList,
                          uint32_t page) {
        if (lruList.getSize() == tlbSize) {  // If TLB is full
            uint32_t lruPage = lruList.getBack();  // Get the least recently used page (back of the list)
            lruList.popBack();  // Remove the least recently used page from the list
            cache.erase(lruPage);  // Remove it from the cache
        }
        lruList.pushFront(page);  // Add the new page to the front of the LRU list
        cache[page] = lruList.begin().getNode();  // Update the cache with the new page's position
    }

    // Function to simulate LRU page replacement strategy
    int simulateLRU(const uint32_t* pageNumbers, int numAccesses) {
        unordered_map<uint32_t, Node<uint32_t>*> cache;  // Cache map to track page positions
        MyDS<uint32_t> lruList;  // LRU list
        int hits = 0;  // Count of TLB hits

        for (int i = 0; i < numAccesses; i++) {
            uint32_t page = pageNumbers[i];  // Get the current page

            if (cache.find(page) != cache.end()) {
                handleTLBHitLRU(hits, cache, lruList, page);  // If page is in cache, it's a hit
            } else {
                handleTLBMissLRU(cache, lruList, page);  // Otherwise, it's a miss
            }
        }
        return hits;  // Return the total number of hits
    }

    // ** Optimal Simulation Functions **

    // Helper function to initialize the next position map for future page accesses
    void initializeNextPositions(const uint32_t* pageNumbers, int numAccesses, 
                                unordered_map<uint32_t, int>& nextPosition) {
        for (int i = numAccesses - 1; i >= 0; --i) {
            nextPosition[pageNumbers[i]] = i;  // Store the future position of each page
        }
    }

    // Helper function to handle a TLB hit in the Optimal strategy
    void handleTLBHitOpt(unordered_map<uint32_t, bool>& tlbEntries, uint32_t page, int& hits) {
        if (tlbEntries.find(page) != tlbEntries.end()) {  // If page is in TLB, it's a hit
            hits++;
        }
    }

    // Helper function to find the page to replace in the Optimal strategy
    uint32_t findPageToReplace(const MyDS<uint32_t>& tlb, 
                                const unordered_map<uint32_t, int>& nextPosition) {
        int farthest = -1;  // Track the farthest future use
        uint32_t replaceValue = 0;  // Page to replace

        for (auto it = tlb.begin(); it != tlb.end(); ++it) {
            uint32_t currentPage = *it;  // Get the current page in the TLB
            int futureIdx = nextPosition.at(currentPage);  // Get the future index of the page

            // If future index is -1, replace this page (not accessed in future)
            if (futureIdx == -1) {
                replaceValue = currentPage;
                break;
            }

            // Otherwise, find the page with the farthest future use
            if (futureIdx > farthest) {
                farthest = futureIdx;
                replaceValue = currentPage;
            }
        }
        return replaceValue;  // Return the page to replace
    }

    // Helper function to handle a TLB miss in the Optimal strategy
    void handleTLBMissOpt(unordered_map<uint32_t, bool>& tlbEntries, 
                    MyDS<uint32_t>& tlb, 
                    const unordered_map<uint32_t, int>& nextPosition, 
                    uint32_t page) {
        if (tlb.getSize() < tlbSize) {  // If TLB is not full
            tlb.pushBack(page);  // Add the new page to the TLB
            tlbEntries[page] = true;  // Mark the page as being in the TLB
        } else {
            // Find the page to replace
            uint32_t replaceValue = findPageToReplace(tlb, nextPosition);

            // Remove the replaced page from TLB and the entries map
            tlbEntries.erase(replaceValue);
            for (auto it = tlb.begin(); it != tlb.end(); ++it) {
                if (*it == replaceValue) {
                    tlb.removeNode(it.getNode());  // Remove the node representing the replaced page
                    break;
                }
            }

            // Add the new page to the TLB
            tlb.pushBack(page);
            tlbEntries[page] = true;
        }
    }

    
    // Function to simulate Optimal page replacement strategy
    int simulateOptimal(const uint32_t* pageNumbers, int numAccesses) {
        unordered_map<uint32_t, int> tlbEntries;  // Map from page to its next use position
        int* nextUse = new int[numAccesses];      // Dynamically allocated array for next use positions
        unordered_map<uint32_t, int> lastSeen;    // Map to track the next occurrence of each page

        // Initialize nextUse array by processing the pageNumbers array backwards
        for (int i = numAccesses - 1; i >= 0; --i) {
            uint32_t page = pageNumbers[i];
            if (lastSeen.find(page) != lastSeen.end()) {
                nextUse[i] = lastSeen[page];  // Next use position of the page
            } else {
                nextUse[i] = numAccesses + 1; // Use a large value if the page is not used again
            }
            lastSeen[page] = i;  // Update the last seen position of the page
        }

        int hits = 0;  // Count of TLB hits

        // Simulate TLB accesses
        for (int i = 0; i < numAccesses; ++i) {
            uint32_t page = pageNumbers[i];  // Current page being accessed
            int next_use = nextUse[i];       // Next use position of the current page

            if (tlbEntries.find(page) != tlbEntries.end()) {
                // TLB hit: Update the next use of the page in TLB entries
                hits++;
                tlbEntries[page] = next_use;
            } else {
                // TLB miss
                if ((int)tlbEntries.size() < tlbSize) {
                    // TLB is not full: Add the page to TLB entries
                    tlbEntries[page] = next_use;
                } else {
                    // TLB is full: Find the page with the farthest next use to replace
                    uint32_t pageToReplace = 0;
                    int maxNextUse = -1;
                    for (const auto& entry : tlbEntries) {
                        if (entry.second > maxNextUse) {
                            maxNextUse = entry.second;
                            pageToReplace = entry.first;
                        }
                    }
                    // Replace the page with the farthest next use
                    tlbEntries.erase(pageToReplace);
                    tlbEntries[page] = next_use;
                }
            }
        }

        delete[] nextUse;  // Free the allocated memory

        return hits;  // Return the total number of TLB hits
    }


public:
    // Function to process input and run the TLB simulation
    void processInput() {
    int T;  // Number of test cases
    cin >> T;

    while (T--) {  // Loop through each test case
        int S, P, K, N;
        cin >> S >> P >> K >> N;
        tlbSize = K;  // Set the TLB size for this test case

        uint32_t* pageNumbers = new uint32_t[N];  // Array to store page numbers
        for (int i = 0; i < N; i++) {
            uint32_t addr;  // Variable to store the address
            cin >> hex >> addr;  // Read hexadecimal value directly into uint32_t
            pageNumbers[i] = getPageNumber(addr, P);  // Calculate page number
        }
        cin >> dec;  // Reset input stream to decimal mode if needed later

        // Output the results of the different simulation strategies
        cout << simulateFIFO(pageNumbers, N) << " "
             << simulateLIFO(pageNumbers, N) << " "
             << simulateLRU(pageNumbers, N) << " "
             << simulateOptimal(pageNumbers, N) << endl;

        delete[] pageNumbers;  // Clean up dynamically allocated array
    }
}

};

// Main function
int main() {
    ios_base::sync_with_stdio(false);  // Disable synchronization with C-style IO for performance
    cin.tie(NULL);  // Untie cin from cout to speed up input processing

    TLBSimulator simulator;  // Create an instance of TLBSimulator
    simulator.processInput();  // Process the input and run the simulations

    return 0;
}
