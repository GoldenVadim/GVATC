class number_of_pages{
private:
    unsigned &page_size;
public:
    unsigned get(const unsigned &image_size);
    number_of_pages(unsigned &value) : page_size(value){}
};