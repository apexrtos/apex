#ifndef dev_mmc_bitfield_h
#define dev_mmc_bitfield_h

namespace mmc {

/*
 * mmc::bitfield
 */
template<typename E>
class bitfield final {
public:
	bitfield(unsigned);

	bool is_set(E v) const;

private:
	unsigned r_;
};

/*
 * implementation
 */
template<typename E>
bitfield<E>::bitfield(unsigned r)
:r_{r}
{ }

template<typename E>
bool
bitfield<E>::is_set(E v) const
{
	return r_ & 1u << static_cast<int>(v);
}

}

#endif
