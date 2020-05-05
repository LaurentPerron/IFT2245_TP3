/**
 * William Bach 20130259
 * Laurent Perron 1052137
 */


#ifndef CONF_H
#define CONF_H

enum {
  NUM_FRAMES = 32,
  NUM_PAGES = 256,
  PAGE_FRAME_SIZE = 256,
  TLB_NUM_ENTRIES = 8,
  PHYSICAL_MEMORY_SIZE = (NUM_FRAMES * PAGE_FRAME_SIZE)
};

#endif
