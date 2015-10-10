INCLUDE = include
P7DIR = p7
EV1DIR = ev1
S1DIR = s1

.PHONY: p7

p7:
	$(MAKE) -C $(P7DIR)

p7clean:
	$(MAKE) -C $(P7DIR) clean
