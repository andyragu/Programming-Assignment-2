The README.txt file must include:
================================
- Each Team Member Information( Name, PID, FIU email, section <RVC, RVD, RVE, U01, U02> )
Andres Agudelo 6295018 aagud018@fiu.edu
Kevin Champagne 6258185 kcham046@fiu.edu
Timothy Rocha 6445055 troch008@fiu.edu
Elijah Chin 6449150 echin027@fiu.edu

- Any specific guidelines, notes, or considerations about the project compilation and execution.
POSIX Message Queues (MQs) align with the assignment’s design by enabling efficient, asynchronous communication between multiple clients and the server. The server processes each client request in a separate thread, ensuring non-blocking execution, while shell commands run in child processes to maintain concurrency. MQs provide structured, FIFO-ordered messaging without requiring complex synchronization, making them ideal for a multi-client, multi-threaded system.

- Brief explanation about how your selected IPC mechanism(s) align with the design and requirements of this assignment, and why they are the most appropriate choice for your implementation.
POSIX Message Queues (MQs) align with the assignment’s design by enabling efficient, asynchronous communication between multiple clients and the server. The server processes each client request in a separate thread, ensuring non-blocking execution, while shell commands run in child processes to maintain concurrency. MQs provide structured, FIFO-ordered messaging without requiring complex synchronization, making them ideal for a multi-client, multi-threaded system.
*** Please refer to the course syllabus for additional assignment submission requirements and guidelines.