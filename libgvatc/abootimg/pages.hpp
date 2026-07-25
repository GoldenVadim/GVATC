class number_of_pages{
private:
    const unsigned short &page_size;
public:
    unsigned get(const unsigned &image_size) const;
    number_of_pages(const unsigned short &value) : page_size(value){}
};