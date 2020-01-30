char data[] = {0x45, 0x43, 0x45, 0x33, 0x35, 0x37};
main()
{
    data[0] = 0x45; //make it dirty
    char *map1 = mmap((void*)0x08044000, 0x2000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    fprintf(stderr, "%c\n", *(map1+0x1000));
    char *map2 = mmap((void*)0x40001000, 0x3000, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    fprintf(stderr, "%c\n", *(map2+0x1000));

}