/**
 * William Bach 20130259
 * Laurent Perron 1052137
 */


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
static unsigned int lru_stack[32];
static unsigned int stack_top = 0;
static FILE* vmm_log;

void vmm_init (FILE *log)
{
  // Initialise le fichier de journal.
  vmm_log = log;
}

void print_stack() {
  printf("[ ");
  for(int i = 0; i < 31; i++) {
    printf("%d, ", lru_stack[i]);
  }
  printf("%d ]\n", lru_stack[31]);
}

int push(int page) {
  int temp1 = lru_stack[0];
  int temp2;
  for(int i = 0; i+1 <= stack_top; i++) {
    if(temp1 == page) break;
    temp2 = lru_stack[i+1];
    lru_stack[i+1] = temp1;
    temp1 = temp2;
  }
  lru_stack[0] = page;
  if(temp1 == page) return -1;
  else return temp1; // la page de la frame qui a ete ejectee
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
  frame_number = tlb_lookup(page_number, 1);
  if(frame_number > -1) {

    printf("Page number %d in tlb\n", page_number);

    paddress = (frame_number << 8) + offset;
    c = pm_read(paddress);
    push(page_number);

  } else { // Si on a un miss on regarde dans le page_table
    frame_number = pt_lookup(page_number);
    if(frame_number > -1) {

      printf("Page number %d in Page table\n", page_number);
      tlb_add_entry(page_number, frame_number, pt_readonly_p(page_number));
      paddress = (frame_number << 8) + offset;
      c = pm_read(paddress);
      push(page_number);

    } else { // Si on a encore un miss, on va chercher directement dans le BACKING_STORE

      printf("Page number %d not in Page table or tlb\n", page_number);
      // Trouver un frame libre
      frame_number = get_download_count();

      if(frame_number > 31) { // On se sert alors de l'algo de remplacement de page

        int page_to_sack = lru_stack[stack_top];
        printf("Page number %d to be sacked\n", page_to_sack);
        frame_number = pt_lookup(page_to_sack);

        if(!pt_readonly_p(page_to_sack))
          pm_backup_page(frame_number, page_to_sack);

        pt_unset_entry(page_to_sack);
      }

      pm_download_page(page_number, frame_number);
      pt_set_entry(page_number, frame_number);
      tlb_add_entry(page_number, frame_number, 1);

      paddress = (frame_number << 8) + offset;
      c = pm_read(paddress);
      if(stack_top < 31) stack_top++;
      push(page_number);
    }
  }
  print_stack();
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
  frame_number = tlb_lookup(page_number, 0);
  if(frame_number > -1) {
    printf("Page number %d in tlb\n", page_number);

    paddress = (frame_number << 8) + offset;
    pm_write(paddress, c);
    pt_set_readonly(page_number, 0);
    push(page_number);

  } else { // Si on a un miss on regarde dans le page_table
    frame_number = pt_lookup(page_number);
    if(frame_number > -1) {

      printf("Page number %d in Page table\n", page_number);
      pt_set_readonly(page_number, 0);
      tlb_add_entry(page_number, frame_number, 0);
      paddress = (frame_number << 8) + offset;
      pm_write(paddress, c);
      push(page_number);

    } else { // Si on a encore un miss, on va chercher directement dans le BACKING_STORE

      printf("Page number %d not in Page table or tlb\n", page_number);
      // Trouver un frame libre
      frame_number = get_download_count();

      if(frame_number > 31) {

        int page_to_sack = lru_stack[stack_top];
        printf("Page number %d to be sacked\n", page_to_sack);
        frame_number = pt_lookup(page_to_sack);

        if(!pt_readonly_p(page_to_sack))
          pm_backup_page(frame_number, page_to_sack);

        pt_unset_entry(page_to_sack);

      }

      pm_download_page(page_number, frame_number);
      pt_set_entry(page_number, frame_number);
      pt_set_readonly(page_number, 0);
      tlb_add_entry(page_number, frame_number, 0);

      paddress = (frame_number << 8) + offset;
      pm_write(paddress, c);
      if(stack_top < 31) stack_top++;
      push(page_number);
    }
  }
  print_stack();
  vmm_log_command (stdout, "WRITING", laddress, page_number, frame_number, offset, paddress, c);
}


// NE PAS MODIFIER CETTE FONCTION
void vmm_clean (void)
{
  fprintf (stdout, "VM reads : %4u\n", read_count);
  fprintf (stdout, "VM writes: %4u\n", write_count);
}
