#ifndef _xml_c_
#define _xml_c_

/**
 *
 *  XML Parser/Generator structural Definitions
 * 
 * 
 *  Author : Gopal Ananthraman
 *  Release Version: 1.0
 */

/* Type declarations */

/* Type Enum */
typedef enum xml_type_t {
	ELEMENT = 0,
	TEXT,
	CDATA,
	COMMENT,
	DOCUMENT,
	ENTITY,
	PI
} xml_type;

/* Clean up related */
enum {
	FREENAME = 0x0001,
    FREETEXT = 0x0002
};

/* XML Attribute */
typedef struct xml_attribute_t {
  struct xml_attribute_t * next;

  char *name;
  char *value;
 
} xml_attribute;

/* Our Node */
typedef struct xml_node_t {
   struct xml_node_t * sibling;
   struct xml_node_t * child;
   struct xml_node_t * parent;

   xml_type type;
   char * name;
   char * text;

   xml_attribute* attributes;
   
} xml_node;

/* XML Element */
typedef struct xml_node_t xml_element;

/* Affects parse buffering */
#define BUFFER_SIZE  2048
#define HALF_SIZE 1024
#define ERROR_BUFFER_SIZE 80

#define RAISE_ERROR(c,c1,s,t,a) \
 if(c < 0 && c != -19) { return raise_error(c1, location(s), t, a); }

// Error codes
#define ENDOFFILE  -19
#define FILEERROR  -18

/* For configuring the parser */
typedef struct config_t {
  /* only flag supported */
  int parsecomment;   /* 1 => parse comments and make comment nodes */
#ifdef DEBUG
  int nodeflush;  /* set to 1 */
  void * fp;      /* pointer to file to write nodes to , while parsing */
#endif
} config_t;

typedef struct stream_t {
	int length;
    int runlength;
	char * buf;
	FILE * fp;
	int activezone;
	int lastactivezone;
	config_t config;
} stream_t;

typedef int (*pfn_end_token)(stream_t * stream);


/* Function declarations */

/* use default config and parse */
int parsexml(void * fp, xml_element ** root);

/* use passed in config */
int parse(void * fp, xml_element ** root, config_t config);

/* mostly private, not for public use */
int parse_node(stream_t* stream, xml_node* parent);
int scan_comment(stream_t * stream);
int parse_attributes(stream_t * stream, xml_element * elt,
					 int * pchild);
int parse_attr(char * q, xml_element * elt);
int parse_element(stream_t * stream, xml_element * parent);
int parse_cdata(stream_t * stream, xml_node * parent);
int parse_text(stream_t * stream,  xml_element * parent);
int parse_comment(stream_t * stream, xml_node * parent);
int parse_entity(stream_t * stream, xml_node * parent);
int parse_PI(stream_t * stream, xml_node * parent);

/* creation */
xml_element * create_element(char * name);
xml_node * create_text(char * text);
xml_node * create_document();
xml_node * create_CDATA(char * text);
xml_node * create_comment(char * text);
xml_node * create_splnode(xml_type type, char * text);
xml_node * create_entity(char * name);
xml_node * create_PI(char * text);
xml_attribute * create_attribute(char * name, 
								 char * value);
void add_attribute(xml_element * element,
			       xml_attribute * attrib);

/* utilities */
void print(xml_node * node, void *fp, int depth);

/* clean up */
void destroy_element(xml_element * e);
void remove_attribute(xml_element * element,
    			      char * name);
void  add_childorsibling(xml_node * parent, xml_node * childorsibling);
xml_node * remove_childorsibiling(xml_node * parent, 
                                  xml_node * childorsibling);

/* XPaths & normalization */
xml_node** select_nodes(xml_node* current, int *pcount, char * xpath);
char * get_attrib_value(xml_node * current, char *xpath);
xml_attribute * find_attribute(xml_node * node, char * name , char * value);
void normalize(xml_node * node);


/* Privates - usage strongly discouraged */
char * process_spl_chars(char * value) ;
int read_text(stream_t * stream, int endchar, pfn_end_token is_endtoken, char ** ptext);
char * location(stream_t * stream);
xml_node** select(xml_node ** current, int *pcount, char * path);
void parse_name_value(char *p, char**name, char ** value);
int skip_whitespaces(stream_t * stream);
/* privates  */


#endif /*_xml_c */