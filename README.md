# Console application which demonstrates use of builtin types


## Publisher

The publisher creates two topics, one of type of `StringTopicType` named "StringTopic" and another of type BytesTopicType named "BytesTopic".

To supply some data for these two topics, the publisher reads a `fortune` file into a string vector and creates a deque of unsigned char of length 256 initially each byte has the same value as the position.

Every three seconds, the publisher writes a random fortune to the "StringTopic" and the contents of the byte deque to the "BytesTopic". The deque is then "shuffled" by pushing the front value onto the back. 


## Subscriber

The subscriber subscribes to two topics, one of type of `StringTopicType` named "StringTopic" and another of type BytesTopicType named "BytesTopic". 

A simple read condition is created for each DataReader which handles the sample output. One thing to notice is that with the built-in stream writer for the sample data doesn't exactly work as expected. 

1. The `BytesTopicType` sample output is signed, while the expectation was for it to be unsigned. To handle this a custom output routine was written.

2. The `StringTopicType` sample output prints the data twice, I assume to show the original data and the processed string, but in this case it looked like a coding error. To handle this, the output is handled by custom code.

To avoid potential issues when writing to the console, a mutex was used to ensure exclusive access.