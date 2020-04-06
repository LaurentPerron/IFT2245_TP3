#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "vmm.h"
#include "tlb.h"
#include "pt.h"
#include "pm.h"

static unsigned int read_count = 0;
static unsigned int write_count = 0;
static FILE* vmm_log;

void vmm_init (FILE *log)
{
  // Initialise le fichier de journal.
  vmm_log = log;
}


// NE PAS MODIFIER CETTE FONCTION
static void vmm_log_command (FILE *out, const char *command,
                             unsigned int laddress, /* Logical address. */
		             unsigned int page,
                             unsigned int frame,
                             unsigned int offset,
                             unsigned int paddress, /* Physical address.  */
		             char c) /* Caractère lu ou écrit.  */
{
  if (out)
    fprintf (out, "%s[%c]@%05d: p=%d, o=%d, f=%d pa=%d\n", command, c, laddress,
	     page, offset, frame, paddress);
}

/* Effectue une lecture à l'adresse logique `laddress`.  */
char vmm_read (unsigned int laddress)
{
  int frame_number;
  int paddress;
  char c = '!';
  read_count++;

  int page_number = laddress >> 8;
  int offset = laddress - (page_number << 8);

  // On cherche dans le tlb
  frame_number = tlb_lookup(page_number, 0);
  if(frame_number > -1) {

    paddress = (frame_number << 8) + offset;
    c = pm_read(paddress);

  } else { // Si on a un miss on regarde dans le page_table
    frame_number = pt_lookup(page_number);
    if(frame_number > -1) {

      paddress = (frame_number << 8) + offset;
      c = pm_read(paddress);
    } else { // Si on a encore un miss, on va chercher directement dans le BACKING_STORE

      // Trouver un frame libre
      frame_number = get_download_count() % 32;

      pm_download_page(page_number, frame_number);
      pt_set_entry(page_number, frame_number);
      tlb_add_entry(page_number, frame_number, 1);

      paddress = (frame_number << 8) + offset;
      c = pm_read(paddress);
    }
  }
  vmm_log_command (stdout, "READING", laddress, page_number, frame_number, offset, paddress, c);
  return c;
}

/* Effectue une écriture à l'adresse logique `laddress`.  */
void vmm_write (unsigned int laddress, char c)
{
  write_count++;
  int frame_number;
  int paddress;

  int page_number = laddress >> 8;
  int offset = laddress - (page_number << 8);

  // On cherche dans le tlb
  frame_number = tlb_lookup(page_number, 1);
  if(frame_number > -1) {

    paddress = (frame_number << 8) + offset;
    pm_write(paddress, c);

  } else { // Si on a un miss on regarde dans le page_table
    frame_number = pt_lookup(page_number);
    if(frame_number > -1) {

      paddress = (frame_number << 8) + offset;
      pm_write(paddress, c);
    } else { // Si on a encore un miss, on va chercher directement dans le BACKING_STORE

      // Trouver un frame libre
      frame_number = get_download_count() % 32;

      pm_download_page(page_number, frame_number);
      pt_set_entry(page_number, frame_number);
      tlb_add_entry(page_number, frame_number, 0);

      paddress = (frame_number << 8) + offset;
      pm_write(paddress, c);
    }
  }
  vmm_log_command (stdout, "WRITING", laddress, page_number, frame_number, offset, paddress, c);
}


// NE PAS MODIFIER CETTE FONCTION
void vmm_clean (void)
{
  fprintf (stdout, "VM reads : %4u\n", read_count);
  fprintf (stdout, "VM writes: %4u\n", write_count);
}
