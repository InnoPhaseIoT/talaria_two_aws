#ifndef DTOA_H
#define DTOA_H

void freedtoa(char *s);
char* dtoa_r(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve, char *buf, size_t blen);
char* dtoa(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve);

#endif /* DTOA_H */
