#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

/**
 * find the right frame to write value
 * @param virtualAddress
 * @param numOfBitsInP num of bit in every p^i address
 * @return frame
 */
word_t getFrameOfVirtualAddress(uint64_t virtualAddress, uint64_t numOfBitsInP);
/**
 * find empty frame to write value
 * @param current frame that we dont want to return
 * @return empty frame if exist
 */
int findEmptyFrame(word_t current);
/**
 * run on frames that already connect to root of tree by recursion, and return the empty frame
 * @param frame 0
 * @param depth 0
 * @param current current address
 * @param address 0
 * @param flag
 * @return num of empty frame if exist, -1 if not
 */
int dfs1(word_t frame, int depth, word_t current, uint64_t address, bool &flag);
/**
 * run of all tree by recursion, and return the max frame that possessed
 * @param frame 0
 * @param depth 0
 * @param max the max frame that already possessed
 */
void dfs2(word_t frame, int depth, int &max);
/**
 * built the address of frame, by add bits to the and of address
 * @param address old address
 * @param depth right place to add
 * @param indexOfFrame bit to add to old address
 */
void constructAddressOfFrame (uint64_t &address, int depth, word_t indexOfFrame);
/**
 * main function that frame to free its old value and add new one
 * @param virtualAddress the address of the father
 * @param currentDepth current depth in tree
 * @return right frame
 */
word_t findFrameToFreeWrapper(uint64_t virtualAddress, int currentDepth);
/**
 * find the frame that we looking for
 * @param indexOfFrame
 * @param depth
 * @param address
 * @param maxAddress
 * @param maxWeight
 * @param maxDepth
 * @param even
 * @param odd
 * @param virtualAddress
 * @param howFarToShift
 */
void findFrameToFree(word_t indexOfFrame, int depth, uint64_t address, uint64_t &maxAddress, unsigned int
&maxWeight, int &maxDepth, unsigned int even, unsigned odd, uint64_t virtualAddress, int howFarToShift);
/**
 * check if the address is max
 * @param even counter
 * @param odd counter
 * @param address address
 * @param maxAddress virtual address of max
 * @param maxWeight max weight
 * @param maxDepth depth
 * @param virtualAddress virtual address of father
 * @param i depth in tree
 * @param depth current depth
 */
void checkMax(unsigned int even, unsigned int odd, uint64_t &address, uint64_t &maxAddress, unsigned int
&maxWeight,int &maxDepth, uint64_t virtualAddress, int i, int depth);
/**
 * read from frame exist value
 * @param virtualAddress address
 * @param depth depth
 * @return right frame
 */
word_t simpleVMread(uint64_t virtualAddress, int depth);
/**
 * free all lines of the frame that we choose as right frame
 * @param maxAddress virtual address
 * @param depthOfMax depth
 */
void unlinkMax(uint64_t maxAddress, int depthOfMax);

void clearTable(uint64_t frameIndex);
/**
 * create num with ones in size of chunk of p^i address
 * @param numOfBitsInP num of ones
 * @return ones
 */
uint64_t createOnes(uint64_t numOfBitsInP);



void VMinitialize() {
    clearTable(0);
}

void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

int VMwrite(uint64_t virtualAddress, word_t value) {
	if(virtualAddress >=  VIRTUAL_MEMORY_SIZE || (int) virtualAddress < 0){
		return 0;
	}
	if(TABLES_DEPTH == 0){
		word_t  indexOfFrame = getFrameOfVirtualAddress(virtualAddress, 0);
		PMrestore(indexOfFrame, virtualAddress/PAGE_SIZE);
		PMwrite(virtualAddress, value);
		return 1;
	}
	else{
		uint64_t numOfBitsInP = CEIL((double)(((double)(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH))/(double)TABLES_DEPTH));
		word_t  indexOfFrame = getFrameOfVirtualAddress(virtualAddress, numOfBitsInP);
		uint64_t onesInLSB = createOnes(OFFSET_WIDTH);
		uint64_t offset = virtualAddress & onesInLSB;
		PMrestore((indexOfFrame), virtualAddress/PAGE_SIZE);
		PMwrite((indexOfFrame * PAGE_SIZE) + offset, value);
		return 1;
	}

}

int VMread(uint64_t virtualAddress, word_t* value) {
	if(virtualAddress >=  VIRTUAL_MEMORY_SIZE || (int) virtualAddress < 0){
		return 0;
	}
	if(TABLES_DEPTH == 0){
		word_t  indexOfFrame = getFrameOfVirtualAddress(virtualAddress, 0);
		PMrestore(indexOfFrame, virtualAddress/PAGE_SIZE);
		PMread(virtualAddress, value);
		return 1;
	}
	uint64_t numOfBitsInP = CEIL((double)(((double)(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH))/(double)TABLES_DEPTH));
    word_t indexOfFrame = getFrameOfVirtualAddress(virtualAddress, numOfBitsInP);
    uint64_t onesInLSB = createOnes(OFFSET_WIDTH);
    uint64_t offset = virtualAddress & onesInLSB;
    PMrestore((indexOfFrame), virtualAddress/PAGE_SIZE);
    PMread((indexOfFrame * PAGE_SIZE) + offset, value);
    return 1;
}

word_t getFrameOfVirtualAddress(uint64_t virtualAddress, uint64_t numOfBitsInP){
    word_t indexOfFrame = 0;
    word_t valueInFrame;
    uint64_t onesInLSB = createOnes(numOfBitsInP);
    for (int i = TABLES_DEPTH; i > 0; i--) {
        uint64_t shiftedVirtualAddress = virtualAddress >> ((numOfBitsInP * (i -1)) + OFFSET_WIDTH);
        uint64_t P_Address = shiftedVirtualAddress & onesInLSB;
        PMread((uint64_t)((indexOfFrame * PAGE_SIZE) + P_Address), &valueInFrame);
        if (valueInFrame == 0){
            word_t indexOfEmptyFrame = findEmptyFrame(indexOfFrame);
            if (indexOfEmptyFrame >= NUM_FRAMES){
                indexOfEmptyFrame = findFrameToFreeWrapper(virtualAddress, i);
            }
            PMwrite((uint64_t)((indexOfFrame * PAGE_SIZE) + P_Address), indexOfEmptyFrame);
            indexOfFrame = indexOfEmptyFrame;
            for(int j = 0; j < PAGE_SIZE; j++){
                PMwrite((indexOfEmptyFrame*PAGE_SIZE + j), 0);
            }
        }
        else{
            indexOfFrame = valueInFrame;
        }
    }
    return indexOfFrame;
}

word_t findFrameToFreeWrapper(uint64_t virtualAddress, int currentDepth){
    word_t indexToStartSearch = 0;
    uint64_t address = 0;
    uint64_t maxAddress = 0;
    unsigned int maxWeight = 0;
    int maxDepth = 0;
    unsigned int even = 1;
    unsigned odd = 0;
    findFrameToFree(indexToStartSearch, TABLES_DEPTH, address, maxAddress, maxWeight, maxDepth, even,
    		odd, virtualAddress, currentDepth);
    word_t frame = simpleVMread(maxAddress, maxDepth);
    unlinkMax(maxAddress, maxDepth);
    uint64_t VirtualPage = maxAddress / PAGE_SIZE;
    PMevict(frame, VirtualPage);
    return frame;
}


void findFrameToFree(word_t indexOfFrame, int depth, uint64_t address, uint64_t &maxAddress,
		unsigned int &maxWeight, int &maxDepth, unsigned int even, unsigned odd, uint64_t virtualAddress, int howFarToShift) {
    if(depth == 0){
        ((address/PAGE_SIZE) % 2) == 0 ? even++ : odd++;
        checkMax(even, odd, address, maxAddress, maxWeight, maxDepth, virtualAddress, howFarToShift, depth);
        return;
    }
    word_t frameBlock = 0;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        PMread((uint64_t) ((indexOfFrame * PAGE_SIZE) + i), &frameBlock);
        int wasEven = 0;
        int wasOdd = 0;
        uint64_t addressAdded = 0;
        if(frameBlock != 0){
            frameBlock % 2 == 0 ? wasEven++ : wasOdd++;
        }
        if (frameBlock == 0) {
            continue;
        }
        constructAddressOfFrame(addressAdded, depth, i);
        findFrameToFree(frameBlock, depth - 1, address + addressAdded, maxAddress, maxWeight, maxDepth,
        		even+wasEven, odd+wasOdd, virtualAddress,
        		howFarToShift);
    }
}

void constructAddressOfFrame (uint64_t &address, int depth, word_t indexOfFrame){
    uint64_t temp = (uint64_t)indexOfFrame << ((uint64_t)((depth - 1) * CEIL((double)(((double)(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH))/(double)TABLES_DEPTH))+ OFFSET_WIDTH));
    address = address | temp;
}

void checkMax(unsigned int even, unsigned int odd, uint64_t &address, uint64_t &maxAddress, unsigned int &maxWeight,int &maxDepth, uint64_t virtualAddress, int i, int depth){
    if((((even * WEIGHT_EVEN) + (odd * WEIGHT_ODD)) > maxWeight) ||
       ((((even * WEIGHT_EVEN) + (odd * WEIGHT_ODD))  ==  maxWeight) && address < maxAddress)){
        maxWeight = (even * WEIGHT_EVEN) + (odd * WEIGHT_ODD);
        maxAddress = address;
        maxDepth = TABLES_DEPTH - depth;
    }
}

void unlinkMax(uint64_t maxAddress, int depthOfMax){
    uint64_t numOfBitsInP = CEIL((double)(((double)(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH))/(double)TABLES_DEPTH));
    uint64_t onesInLSB = createOnes(numOfBitsInP);
    word_t valueInFrame;
    word_t indexOfFrame = 0;
    int index = TABLES_DEPTH;
    for (int i = depthOfMax; i > 0 ; i--) {
        uint64_t shiftedVirtualAddress = maxAddress >> (numOfBitsInP * (index-1) + OFFSET_WIDTH);
        uint64_t P_Address = shiftedVirtualAddress & onesInLSB;
        if (i == 1){
            PMwrite(indexOfFrame * PAGE_SIZE + P_Address, 0);
            break;
        }
        PMread((uint64_t)((indexOfFrame * PAGE_SIZE) + P_Address), &valueInFrame);
        indexOfFrame = valueInFrame;
        index--;
    }
}


word_t simpleVMread(uint64_t virtualAddress, int depth) {
    uint64_t numOfBitsInP = CEIL((double)(((double)(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH))/(double)TABLES_DEPTH));
    uint64_t onesInLSB = createOnes(numOfBitsInP);
    word_t valueInFrame;
    word_t indexOfFrame = 0;
    for (int i = depth; i > 0; i--) {
        uint64_t shiftedVirtualAddress = virtualAddress >> ((numOfBitsInP * (i -1)) + OFFSET_WIDTH);
        uint64_t P_Address = shiftedVirtualAddress & onesInLSB;
        PMread((uint64_t)((indexOfFrame * PAGE_SIZE) + P_Address), &valueInFrame);
        indexOfFrame = valueInFrame;
    }
    return indexOfFrame;
}


int findEmptyFrame(word_t current){
    int max = 0;
    int depth = 0;
    bool flag = true;
    int frame = dfs1(0, 0, current, 0, flag);
    if (frame != -1){
        return frame;
    }
    dfs2(0, depth, max);
    return max + 1;

}


void dfs2(word_t frame, int depth, int &max){
    if (depth == TABLES_DEPTH){
        return;
    }
    word_t newFrame;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        PMread(i + (frame * PAGE_SIZE), &newFrame);
        if (newFrame != 0){
            if (max < newFrame){
                max = newFrame;
            }
            dfs2(newFrame, depth+1, max);
        }
    }
}

int dfs1(word_t frame, int depth, word_t current, uint64_t address, bool &flag){
    if (depth == TABLES_DEPTH) {
        return -1;
    }
    word_t newFrame;
    int x = 0;
//    uint64_t tempAddress = address;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        PMread(i + (frame * PAGE_SIZE), &newFrame);
        if (newFrame != 0) {
        	uint64_t addToAddress = 0;
            constructAddressOfFrame (addToAddress, TABLES_DEPTH - depth, i);
            x = dfs1(newFrame, depth + 1, current, address+addToAddress, flag);
            if (x != -1 && x != current){
                if (flag){
                    unlinkMax(address,depth);
                    flag = false;
                }
                return x;
            }
        }
    }
    if (x == -1 || frame == current){
        return -1;
    }
    if (flag){
        unlinkMax(address,depth);
        flag = false;
    }
    return frame;
}


uint64_t createOnes(uint64_t numOfBitsInP){
    uint64_t onesBit = 0;
    for(uint64_t i = 0; i < numOfBitsInP; i++){
        onesBit += pow(2, i);
    }
    return onesBit;
}
