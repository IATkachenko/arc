#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

namespace ArcCredential {


char **listjoin(char **base, char **addon, int size) {
  char **store = addon, 
    **storebase = base,
    **newvect = NULL;
  int num = 0, num2=0;
  int i;

  if (!addon || !(*addon))
    return base;

  while (*store++)
    num++;

  if (storebase)
    while (*storebase++)
      num2++;

  if ((newvect = (char **)malloc((num+num2+1)*size))) {
    if (newvect) {
      memcpy(newvect, base, (size*(num2)));
      for (i=num2; i <num+num2; i++)
        newvect[i] = addon[i-num2];
      newvect[i] = NULL;
      free(base);
      return newvect;
    }
   }
  return NULL;
}

char **listadd(char **vect, char *data, int size) {
  int i = 0;
  char **newvect;

  if (!data || (size <= 0))
    return NULL;

  if (vect)
    while (vect[i++]) ;
  else
    i=1;

  if ((newvect = (char **)malloc((i+1)*size))) {
    if (vect) {
      memcpy(newvect, vect, (size*(i-1)));
      newvect[i-1] = data;
      newvect[i] = NULL;
      free(vect);
    }
    else {
      newvect[0] = data;
      newvect[1] = NULL;
    }
    return newvect;
  }
  return NULL;
}

void listfree(char **vect, void (*f)(void *)) {
  char **tmp = vect;
  if (tmp) {
    int i = 0;
    while (tmp[i])
      f(tmp[i++]);
    free(vect);
  }
}


} // namespace ArcCredential

