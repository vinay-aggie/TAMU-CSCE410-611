/*
 File: ContFramePool.C
 
 Author: Vinay Balamurali
 Date  : 01/15/2025
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

ContFramePool * ContFramePool::headPool = nullptr;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

// Parameterized constructor.
ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Bitmap must fit in a single frame!
    assert(_n_frames <= FRAME_SIZE * 8);

    // initialize private members.
    this->base_frame_no = _base_frame_no;
    this->nframes = _n_frames;
    this->info_frame_no = _info_frame_no;
    this->nFreeFrames = this->nframes;

    // Place the Management frame appropriately.
    if (this->info_frame_no == 0)
    {
        this->bitMap = (unsigned char *) (this->base_frame_no * FRAME_SIZE);
    }
    else
    {
        this->bitMap = (unsigned char *) (this->info_frame_no * FRAME_SIZE);
    }

    // Set all frames to free
    for (int frame = 0; frame < this->nframes; frame++)
    {
        set_state(frame, FrameState::Free);
    }

    // Set the info frame as 'used'.
    if (this->info_frame_no == 0)
    {
        set_state(0, FrameState::Used);
        this->nFreeFrames -= 1;
    }

    // Linked list stuff.
    // Add a node at the end.
    // This linked is allocatted in the stack currently.
    if (headPool == nullptr)
    {
        headPool = this;
        headPool->nextPool = nullptr;
    }
    else
    {
        ContFramePool *pool = headPool;

        // Traverse to the end of the pool
        // and a new pool at the end.
        while (pool->nextPool != nullptr)
        {
            pool = pool->nextPool;
        }
        pool->nextPool = this;
    }

    Console::puts("Frame Pool initialized\n");

} // ContFramePool::ContFramePool


ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    unsigned long bitMapIndex = _frame_no / 4;
    unsigned int shift = ((_frame_no % 4) * 2);
    unsigned char mask = 0x03 << shift;
    unsigned char res = ((bitMap[bitMapIndex] >> shift) & 0x03);
    // Console::puts("get_state = "); Console::puti(res); Console::puts("\n");

    if (res == 0x03)
    {
        // Console::puts("Frame is Head of Sequence!\n");
        return FrameState::HoS;
    }
    else if (res == 0x01)
    {
        // Console::puts("Frame is Used!\n");
        return FrameState::Used;
    }
    else if (res == 0x00)
    {
        // Console::puts("Frame is Free!\n");
        return FrameState::Free;
    }

    // Console::puts("Frame state unknown\n");
    return FrameState::Free;

}

// Method to set the state for a given frame no.
// Options are as follows:
// Free: 00
// Used: 01
// Hos:  11
// Hos --> Head of Sequence.
// With 2 bits, we have 4 options, but we are only making use of 3.
void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    // Console::puts("Setting state \n");
    // Parse out the index and exact position of the frame management
    // bits within the index.
    unsigned long bitMapIndex = _frame_no / 4;
    unsigned int shift = ((_frame_no % 4) * 2);
    // Console::puts("_frame_no = "); Console::putui(_frame_no); Console::puts("\n");
    // Console::puts("bitMapIndex = "); Console::putui(bitMapIndex); Console::puts("\n");
    // Console::puts("shift = "); Console::putui(shift); Console::puts("\n");

    switch (_state)
    {
        case FrameState::HoS:
            {
                // Console::puts("Setting state to HoS\n");
                unsigned int clearMask = ~(0x03 << shift);
                bitMap[bitMapIndex] &= clearMask;

                unsigned int setMask = (0x03 << shift);
                bitMap[bitMapIndex] |= setMask;
                break;
            }

        case FrameState::Used:
            {
                // Console::puts("Setting state to used\n");
                unsigned int clearMask = ~(0x03 << shift);
                bitMap[bitMapIndex] &= clearMask;

                unsigned int setMask = (0x01 << shift);
                bitMap[bitMapIndex] |= setMask;
                break;
            }

        case FrameState::Free:
            {
                // Console::puts("Setting state to free\n");
                bitMap[bitMapIndex] &= ~(0x03 << shift);
                break;
            }
    }

} // ContFramePool::set_state


// Method to fetch the requested frames.
unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // check if requested frames are more than the number of free frames.

    if (_n_frames > this->nFreeFrames)
    {
        Console::puts("Failed to allocate frames = "); Console::puti(base_frame_no);
        Console::puts(" . Free frames are lesser than the requested size.\n");
        assert(false);
    }

    unsigned long startIndex = 0;
    unsigned long freeFrameCount = 0;

    // Console::puts("base_frame_no = "); Console::puti(base_frame_no); Console::puts("\n");
    // Console::puts("nframes = "); Console::puti(nframes); Console::puts("\n");
    // Console::puts("_n_frames = "); Console::puti(_n_frames); Console::puts("\n");

    // Attempt to find a sequence of contiguous frames using a single loop only.
    for (unsigned long i = 0; i < this->nframes; i++)
    {
        // Console::puts("iteration = "); Console::puti(i); Console::puts("\n");

        // If we get a match for the number of free frames,
        // we mark them as inaccessible.
        if (freeFrameCount == _n_frames)
        {
            // Console::puts("Got a match for frames = "); Console::puti(_n_frames); Console::puts("\n");
            mark_inaccessible(startIndex, freeFrameCount);
            Console::puts("start index = "); Console::puti(startIndex); Console::puts("\n");
            this->nFreeFrames = this->nFreeFrames - _n_frames;

            // Return frame number of the first frame in this sequence.
            return (startIndex + this->base_frame_no);
        }

        // Every time we hit a a 'HoS' or 'Used' frame, we have to start our search
        // from scratch. Hence, we need to reset our variables.
        if (get_state(i) == FrameState::HoS || get_state(i) == FrameState::Used)
        {
            startIndex = i + 1;
            freeFrameCount = 0;
            continue;
        }

        // Up the free counts.
        freeFrameCount++;
    }

    Console::puts("No match for frames = "); Console::puti(_n_frames); Console::puts("\n");
    return 0;

} // ContFramePool::get_frames


// Mark a series as frames as inaccessible.
// This is done by setting the 'Head of Sequence' as 'HoS'.
// The rest of the frames are marked 'Used'.
void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    set_state(_base_frame_no, FrameState::HoS);
    for (unsigned long frameNo = _base_frame_no + 1; frameNo < _base_frame_no + _n_frames; frameNo++)
    {
        set_state(frameNo, FrameState::Used);
    }

} // ContFramePool::mark_inaccessible


// Method to release a set of frames back to the frame pool.
void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool *node = headPool;

    // Console::puts("Releasing frames starting with: "); Console::putui(_first_frame_no); Console::puts("\n");
    // Console::puts("Base frame: "); Console::putui(node->base_frame_no); Console::puts("\n");
    while (node != nullptr)
    {
        if ((_first_frame_no < node->base_frame_no) ||
            (_first_frame_no >= (node->base_frame_no + node->nframes)))
        {
            Console::puts("Requested frame does not belong to this pool! Check next.\n");
            // Move to the next pool if the frame doesn't belong to this one.
            node = node->nextPool;
            continue;
        }

        // This is the relative index.
        unsigned long index = (_first_frame_no - node->base_frame_no);

        // some basic checks.
        if (node->get_state(index) != FrameState::HoS)
        {
            Console::puts("Incorrect release request!\n");
            assert(false);
        }

        // Set the Head Of Sequence to free.
        node->set_state(index, FrameState::Free);
        node->nFreeFrames += 1;

        // Start checking the sequence
        index += 1;
        while (node->get_state(index) == FrameState::Used)
        {
            node->set_state(index, FrameState::Free);
            node->nFreeFrames += 1;
            index++;
        }

        // Console::puts("Frames released successfully!\n");
        return;
    }

    // If no pool contains the frame, log an error and assert.
    Console::puts("Requested frame does not belong to any pool!\n");
    assert(false);

} // ContFramePool::release_frames


unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    return ((_n_frames * 2) / (8 * 4096)) + (((_n_frames * 2) % (8 * 4096)) > 0 ? 1 : 0);
    
} // ContFramePool::needed_info_frames
