//funzioni utili in c

////////////////// isNumber //////////////////
static long isNumber(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}
