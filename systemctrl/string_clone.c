#include <pspsdk.h>
#include <pspkernel.h>
#include <string.h>
#include <stdlib.h>

static char *__strtok_context;

char *strtok_r_clone(char *string, const char *seps, char **context)
{
	char *head;  /* start of word */
	char *tail;  /* end of word */

	/* If we're starting up, initialize context */
	if(string) *context = string;

	/* Get potential start of this next word */
	head = *context;
	if (head == NULL) return NULL;

	/* Skip any leading separators */
	while(*head && strchr(seps, *head))
	{
		head++;
	}

	 /* Did we hit the end? */
	if (*head == 0)
	{
		/* Nothing left */
		*context = NULL;
		return NULL;
	}

	/* skip over word */
	tail = head;
	while (*tail && !strchr(seps, *tail))
	{
		tail++;
	}

	/* Save head for next time in context */
	if (*tail == 0)
	{
		*context = NULL;
	}
	else
	{
		*tail = 0;
		tail++;
		*context = tail;
	}

	/* Return current word */
	return head;
}

char *strtok_clone(char *str, const char *seps)
{
	return strtok_r_clone(str, seps, &__strtok_context);
}

void atob_clone(char *a0, int *a1)
{
	char *p;
	*a1 = strtol(a0, &p, 10);
}

size_t strspn_clone(const char *s, const char *accept)
{
    const char *c;
    for(c = s; *c; c++)
	{
        if(!strchr(accept, *c)) return c - s;
    }

    return c - s;
}

size_t strcspn_clone(const char *s, const char *reject)
{
    const char *c;
    for(c = s; *c; c++)
	{
        if(strchr(reject, *c)) return c - s;
    }

    return c - s;
}