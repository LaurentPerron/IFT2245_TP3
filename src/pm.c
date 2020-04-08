#include <stdio.h>
#include <string.h>


#include "conf.h"
#include "pm.h"

static FILE *pm_backing_store;
static FILE *pm_log;
// Frame 0 : indices 0 ... 255 ; Frame 1 : indices 256 ... 511 ; etc.
static char pm_memory[PHYSICAL_MEMORY_SIZE]; // 32 frames de tailles 256 char -> 8192 cellules
static unsigned int download_count = 0;
static unsigned int backup_count = 0;
static unsigned int read_count = 0;
static unsigned int write_count = 0;

int get_download_count() {
  return download_count;
}

// Initialise la mémoire physique
void pm_init (FILE *backing_store, FILE *log)
{
  pm_backing_store = backing_store;
  pm_log = log;
  memset (pm_memory, '\0', sizeof (pm_memory));
}

// Charge la page demandée du backing store
void pm_download_page (unsigned int page_number, unsigned int frame_number)
{
  if(pm_backing_store == NULL) {
    printf("backing store not initialized\n");
  }
  download_count++;

  int begin = 256 * frame_number;
  int end = begin + 256;
  // On amene le cursor a la bonne page
  fseek(pm_backing_store, page_number * 256, SEEK_SET);

  for(int i = begin; i < end; i++) {
    pm_memory[i] = (char)fgetc(pm_backing_store);
  }

  rewind(pm_backing_store);

  if(pm_log == NULL) {
    printf("PM Log not initialized\n");
    return;
  }
  fprintf (pm_log, "%s: p=%d, f=%d\n", "DOWNLOADED", page_number, frame_number);
}

// Sauvegarde la frame spécifiée dans la page du backing store
void pm_backup_page (unsigned int frame_number, unsigned int page_number)
{
  backup_count++;

  int begin = 256 * frame_number;
  int end = begin + 256;
  fseek(pm_backing_store, page_number * 256, SEEK_SET);

  for(int i = begin; i < end; i++) {
    fputc(pm_memory[i], pm_backing_store);
  }

  rewind(pm_backing_store);

  if(pm_log == NULL) {
    printf("PM Log not initialized\n");
    return;
  }
  fprintf (pm_log, "%s: p=%d, f=%d\n", "BACKED-UP", page_number, frame_number);
}

char pm_read (unsigned int physical_address)
{
  char c;
  read_count++;
  /* physical address de la forme :
     XXXXX | XXXXXXXX
     frame | offset */

  // On cherche le no de frame et le offset
  int frame_number = physical_address >> 8;
  int offset = physical_address - (frame_number << 8);

  c = pm_memory[frame_number * 256 + offset];
  if(pm_log == NULL) {
    printf("PM Log not initialized\n");
    return c;
  }
  fprintf (pm_log, "%s[%c]@%05d: f=%d, o=%d\n", "READING", c, physical_address, frame_number, offset);
  return c;
}

void pm_write (unsigned int physical_address, char c)
{
  write_count++;
  /* physical address de la forme :
     XXXXX | XXXXXXXX
     frame | offset */

  // On cherche le no de frame et le offset
  int frame_number = physical_address >> 8;
  int offset = physical_address - (frame_number << 8);

  if(pm_log == NULL) {
    printf("PM Log not initialized\n");
    return;
  }
  fprintf (pm_log, "%s[%c]@%05d: f=%d, o=%d\n", "READING", c, physical_address, frame_number, offset);
  pm_memory[frame_number * 256 + offset] = c;
}


void pm_clean (void)
{
  // Enregistre l'état de la mémoire physique.
  if (pm_log)
    {
      for (unsigned int i = 0; i < PHYSICAL_MEMORY_SIZE; i++)
	{
	  if (i % 80 == 0)
	    fprintf (pm_log, "%c\n", pm_memory[i]);
	  else
	    fprintf (pm_log, "%c", pm_memory[i]);
	}
    }
  fprintf (stdout, "Page downloads: %2u\n", download_count);
  fprintf (stdout, "Page backups  : %2u\n", backup_count);
  fprintf (stdout, "PM reads : %4u\n", read_count);
  fprintf (stdout, "PM writes: %4u\n", write_count);
}
