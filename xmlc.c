#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmlc.h"


/*
 * Simple XML parser & generator
 * This is not a conforming parser.
 * supports creation of elements, attributes and text.
 * associating attributes with elements assigning children to 
 * elements parses an  XML. Does not yet support entity parsing and 
 * internal and external entity reference resolution.
 * It however supports default entity reference resolution
 * Is not a validating parser.
 * Nodes created are similar to DOM
 * limited selection through xpath is supported.
 * Care is taken to update 
 * 
 * Author: Gopal Ananthraman
 * Release Version : 1.0
 * 
 */

static char buffer[BUFFER_SIZE];
static char tmp[BUFFER_SIZE];

/* INTERNAL */
void destroy_element(xml_element * e) {
	if(e) {
		free(e->name);
		free(e);
	}
}

/* Use this to free the tree */
void destroy_node(xml_node * node) {
    xml_node *p, *q; 

    if(node == NULL)
        return;

    p = node->child;

    while(p) {
	  q = p->sibling;
      destroy_node(p);
      p = q; 
    }

    if(node->name) {
        free(node->name);
        node->name = NULL;
    }

    if(node->text) {
      free(node->text);
      node->text = NULL;
    }

    free(node);

    /* node's parent is not updated etc...*/
}

/* INTERNAL */
char * location(stream_t * stream) {

   int len = stream->runlength;
   len -= (ERROR_BUFFER_SIZE - 1);

   if(len < 0) {
	   memcpy(&tmp[0], &stream->buf[0], 
	    stream->runlength);

       tmp[stream->runlength - 1] = 0;
   } else {
       memcpy(&tmp[0], &stream->buf[len], 
  		     ERROR_BUFFER_SIZE - 1);
 
       tmp[ERROR_BUFFER_SIZE -1] = 0;
   }

   return &tmp[0];
}

/* INTERNAL */
void unget_c(stream_t * stream, int count) {
	int len = stream->runlength;

	if(count > HALF_SIZE) {
	   count = HALF_SIZE;
	}

	len -= count;

	if(len < 0) {
	  if(stream->lastactivezone == 2)
         len += BUFFER_SIZE;
	  else 
		 len = 0;
	}

	stream->runlength = len;
}

/* INTERNAL */
int get_c(stream_t * stream) {
	char * buf = NULL;
	int size = -1;

	if(stream->length > 0 && stream->length < HALF_SIZE) {
		if((stream->runlength == stream->length) ||
		  (stream->runlength == stream->length + HALF_SIZE))
            return -19; // EOF
	}
     
	if(stream->runlength == HALF_SIZE && stream->activezone == 1) {
       size = HALF_SIZE;
	   stream->activezone = 2;
	   stream->lastactivezone = 1;
	} else if (stream->runlength == BUFFER_SIZE) {
		stream->runlength = 0;
		if(stream->activezone == 2) {
			size = 0; 
			stream->activezone = 1;
			stream->lastactivezone = 2;
		}
	}

	if(size >= 0) {
	   if (feof(stream->fp)) {
	        return -19;
	   }
	   clearerr(stream->fp);

       stream->length = fread(&buffer[size], sizeof(char), 
               HALF_SIZE, stream->fp);
	   if(ferror(stream->fp)){
		   perror("Stream Error");
	 	   return -18;
	   } 
	}


    return stream->buf[stream->runlength++];
}

/* INTERNAL */
int raise_error(int code, char * src, char* error, char* arg1) {
    printf(error, (arg1 ? arg1 : ""));
	printf("Location: %s\n", (src ? src : ""));

	return code;

}

/** 
  * Use this API to parse XML with default configs 
  * fp --> pointer to XML file to read
  * root --> typically allocated memory for the root doc.
  */
int parsexml(void * fp, xml_element ** root) {
	config_t config;
	config.parsecomment = 0L;
	return parse(fp, root, config);
}



/**
  * Use this API to parse XML with passed in configuration 
  * fp --> pointer to XML file to read
  * root --> typically allocated memory for the root doc.
  * config --> configuration properties that affect parsing
  *              not much now, affects say comments parsing
  */
int parse(void * fp, xml_element ** root, config_t config) {
   int len = 0;
   xml_node * document = NULL;
   int ret = 0;

   stream_t *stream = (stream_t *)malloc(sizeof(stream_t));
   stream->buf = &buffer[0];
   stream->runlength = 0;
   stream->activezone = 1;
   stream->lastactivezone = -1;
   stream->fp = (FILE *)fp;
   stream->config = config;

   stream->length = fread(buffer, sizeof(char), 
	                HALF_SIZE, stream->fp);

   if(ferror(stream->fp)) {
	    *root = NULL;
		return -18;
   }

   document = create_document();
   ret = parse_node(stream, document);

   *root = document;
   return ret;
}

/* INTERNAL */
/* Recursive descent begins here */
/* Root point for an XML node parse */
int parse_node(stream_t * stream, xml_node * parent) {
    
	int c,c1,c2,c3;

	for(;;) {

		c = skip_whitespaces(stream);
		RAISE_ERROR(c, -24, stream, "Error while trimming spaces in node", "")

		c = get_c(stream);
		if(c == ENDOFFILE) //EOF is fine
          return 0; 

		if(c == '<') {
			char * name = NULL;
			c1 = get_c(stream);
			RAISE_ERROR(c1, -1, stream, "Incomplete tag", "")
			c2 = get_c(stream);
			RAISE_ERROR(c2, -2, stream, "Incomplete tag", "")
			c3 = get_c(stream);
			RAISE_ERROR(c3, -3, stream, "Incomplete tag", "")

            if(c == -19) 
              return 0; 

			if(c1 == '!' && c2 == '-' && c3 =='-') {
				int ret;
				if(stream->config.parsecomment) {
				  ret = parse_comment(stream, parent); 
				} else {
				  ret = scan_comment(stream);
				}

			   RAISE_ERROR(ret, -4, stream, "Error While scanning for comments", "")
               
			} else {
				unget_c(stream, 4);

				/* This means, We are parsing the end tag at the wrong level, return success */
				if(c1 == '/') { 
					return 0;
				}

				if( '!' == c1) {
				  parse_entity(stream, parent);
				} else if ('?' == c1) {
				  parse_PI(stream, parent);
				} else {
				  parse_element(stream, parent); 
				}
			}
		   
		} else {
 		   unget_c(stream, 1);
		   parse_text(stream, parent);
		}
	}
}

/* INTERNAL */
/* XML Entity */
int parse_entity(stream_t * stream, xml_node * parent) {
  int c = get_c(stream);
  int c1 = get_c(stream);
  char * q = &tmp[0];
  xml_node * entity;
  char * text;

  if(c == '<' && c1 == '!') {
 	for(;;) {
		c = get_c(stream);
		RAISE_ERROR(c, -31, stream, "Invalid Entity name", "")

		if(c <= 0x20) {
          *q = 0;
          break;
		}

		*q++ = c;
	}

	q = &tmp[0];

	if(*q == 0) {
       RAISE_ERROR(-32, -31, stream, "Invalid Entity", "")
	}

	entity = create_entity(q);

    c = read_text(stream, '>', NULL, &text);
	RAISE_ERROR(c, -5, stream, "Invalid Entity values", "");

	entity->text = text;

	add_childorsibling(parent, entity);

#ifdef DEBUG
	if(stream->config.nodeflush) {
		print(entity, stream->config.fp, 0);
	}
#endif

  }
  return 0;
}

/* INTERNAL */
int pi_end_token(stream_t * stream) {
   int c1 = get_c(stream);
   if(c1 < 0)
      return c1;

   if(c1 == '>')
      return 1;   
   
   unget_c(stream, 1);
   return 0;
}

/* INTERNAL.
 *  Processing Instruction parsing 
 */
int parse_PI(stream_t * stream, xml_node * parent) {
  int c = get_c(stream);
  int c1 = get_c(stream);
  int c2 = get_c(stream);
  int c3 = get_c(stream);
  int c4 = get_c(stream);

  char * text = NULL;
  xml_element * elt = NULL;

  if(c == '<' && c1 == '?' && 
	  (c2 =='x' || c2 =='X') &&
	  (c3 =='m' || c3 =='M') &&
	  (c4 =='l' || c4 =='L')) {

	  int child = 0;

	  elt = create_PI("xml");

	  c = read_text(stream, '?', pi_end_token, &text);
	  RAISE_ERROR(c, -5, stream, "Invalid Processing Instruction", "");

	  elt->text = text;
	  add_childorsibling(parent, elt);
#ifdef DEBUG
	  if(stream->config.nodeflush) {
		print(elt, stream->config.fp, 0);
	  }
#endif

	  return 0;
  }

  unget_c(stream, 5);

  return -33;
}

/* INTERNAL */
int cdata_end_token(stream_t * stream) {
	int c1;
    c1 = get_c(stream);
    if(c1 < 0)
	   return c1;

    if(c1 == ']') {
 	   c1 = get_c(stream);
	   if(c1 < 0)
	      return c1;
			  

	   if( c1 == '>') {
		  return 1;
	   } else {
	 	 unget_c(stream, 1);
		 unget_c(stream, 1);
	   }
	} else {
	  unget_c(stream, 1);
	}

	return 0;
}

/* INTERNAL */
/* CDATA Section parsing */
int parse_cdata(stream_t * stream, xml_node * parent) {
	int ch[9];
	int i = 0;
	char * text = NULL;
	xml_node * node;

	for(; i < sizeof(ch); i++) {
		ch[i] = get_c(stream);
		RAISE_ERROR(ch[i], -5, stream, "Incomplete tag", "")
	}

	if(memcmp(ch, "<![CDATA[", sizeof(ch)) == 0) {

		ch[0] = read_text(stream, ']', cdata_end_token, &text);
		RAISE_ERROR(ch[0], -8, stream, "Error while reading CDATA", "")

		node = create_CDATA(text);

	    add_childorsibling(parent, node);

#ifdef DEBUG
		if(stream->config.nodeflush) {
			print(node, stream->config.fp, 0);
		}
#endif

        return 0;
	}

	unget_c(stream, 9);
	

	return -25;
}


/* INTERNAL */
int parse_text(stream_t * stream,  xml_element * parent) {
	char * text = NULL;
	xml_node * node;
	
	int c = read_text(stream, '<', NULL, &text);
	if(c == -10) {
       RAISE_ERROR(c, -10, stream, "Error while looking for text end char for elt %s", 
		   parent->name)
	} else if(c == -11) {
       RAISE_ERROR(c, -11, stream, "Text too big for %s", parent->name)
	}

	RAISE_ERROR(c, c, stream, "Invalid text at %s", parent->name)
	node = create_text(text);

    free(text);    

	add_childorsibling(parent, node);

#ifdef DEBUG
	if(stream->config.nodeflush) {
		print(node, stream->config.fp, 0);
	}
#endif

	return 0;
}


/* INTERNAL */
int read_text(stream_t * stream, int endchar, pfn_end_token is_endtoken, char ** ptext) {
	char * text = NULL;
	int count = 0;
	int size = 0;
	int c;
	int flag = 0L;

	for(;;) {
		   c = get_c(stream);
		   if(c < 0)
			   return -10;

		   /* check for end condition */
		   if(c == endchar) {
			   if(is_endtoken) {
		          flag = is_endtoken(stream);
				  if(flag < 0) return flag; 
			   } else {
                  flag = 1L;
			   }
		   }

		   if(flag == 1L || count == sizeof(tmp)) {
			   int alloclen = size + count 
				   + ((flag == 1L) ? 1 : 0);
			   char * p = realloc(text, alloclen);
			   if(!p) {
				 if(text != NULL) {
			   	    text[size -1] = 0;
				 }
				 *ptext = text;
                 return -11;
			   }

               text = p;
			   memcpy(&text[size], tmp, count);
			   size = alloclen;
			   
               if(flag == 1L) {
				  text[size -1] = 0;
				  *ptext = text;
			      break;
			   }

			   count = 0;
		   }

           tmp[count++] = c;
	  }

      return 0;
}


/* INTERNAL */
/* Element parsed here */
int parse_element(stream_t * stream, xml_element * parent) {
	int c;
    char * q = &tmp[0];
	xml_element * elt;
	int child = 1L;

	c = get_c(stream);
	if( c != '<') {
		RAISE_ERROR(-12, -12, stream, "Element should begin with < symbol", "")
	}
    
	while(1) {
		c = get_c(stream);
		RAISE_ERROR(c, -12, stream, "stream error while reading element", "")

		if(c <= 0x20 || c == '>') {
          *q = 0;
          break;
		}

		*q++ = c;
	}

	if( c == '>') {
		if(q != &tmp[0] && *--q == '/') {
			*q = 0;
			child = 0L;
		} 
	}

	q = &tmp[0];

	if(q == NULL || !*q ) {
       RAISE_ERROR(-13, -13, stream, "Invalid element name", "")
	}
	/* name */
	elt = create_element(q);

	/* attributes */
	if(c <= 0x20) {
         c = parse_attributes(stream, elt, &child);
		 if( c < 0)
			 return c;
	}

	if(child) {

      parse_node(stream, elt);

      q = &tmp[0];

  	  while(1) {
			c = get_c(stream);
			RAISE_ERROR(c, -16, stream, "reading element %s end tag", elt->name)

			if(c == '<') {
			   if((c = get_c(stream)) == '/')
					continue;
			   else {
                 RAISE_ERROR(c, -17, stream, "No End tag for %s", elt->name)
			   }

			}

			if(c == '>') {
			  *q = 0;
			  break;
			}

			*q++ = c;
		}

	  if(strcmp(elt->name, &tmp[0])) {
		RAISE_ERROR(-22, -22, stream, "Invalid end tag for %s", elt->name)
	  }
	}

	add_childorsibling(parent, elt);

#ifdef DEBUG
	if(stream->config.nodeflush) {
		print(elt, stream->config.fp, 0);
	}
#endif

	return 0;
}

/* XML Attributes */
int parse_attributes(stream_t * stream, xml_element * elt, int * pchild) {
	int inquote = 0;
	int c;
	char * q = &tmp[0];

    c = skip_whitespaces(stream);
    RAISE_ERROR(c, -23, stream, "Error while trimming spaces in element %s", elt->name)

 		for(;;) {
			c = get_c(stream);

			RAISE_ERROR(c, -13, stream, 
				 "Stream error while processing for attributes at %s", elt->name)

		   	if( c == '>') {
			   *q = 0;
 			   if(q != &tmp[0] && *--q == '/') {
			   	  *q = 0;
				  *pchild = 0L;
			   }

   			   q = &tmp[0];

			   c = parse_attr(q, elt);

			   RAISE_ERROR(c, -14, , stream, "Invalid attribute %s", &tmp[0])

               break; 
			}

			if( c == '"') {
				inquote = !inquote;
			}

			if(c <= 0x20 && !inquote) {
			  *q = 0;
			  q = &tmp[0];
			  c = parse_attr(q, elt);
			  RAISE_ERROR(c, -15, stream, "Invalid attribute %s", &tmp[0])
              
			  c = skip_whitespaces(stream);
              RAISE_ERROR(c, -23, stream, "Error while trimming spaces in element %s", elt->name)

			  continue;
			}

			*q++ = c;
		}

	return 0;
}

/* INTERNAL */
int skip_whitespaces(stream_t * stream) {
	int c;
	for(;;) {
		  c = get_c(stream);
		  if( c < 0)
			  return c;
 		  if(c <= 0x20) {
			continue;
		  } else {
            unget_c(stream, 1);
		    break;
		  }
	}

	return c;
}

/*
 * Parse attributes
 */
int parse_attr(char * p, xml_element * elt) {

	char * eq; 
	char * val;
	char * q; 

	if(!p || !*p) {
  	   return 0;
	}

	eq = strrchr(p, '=');

    /* + 2 for the quote '"' */
	q = val = ((eq && *(eq + 1)) ? eq + 2: eq);
	
	if(!eq) {
		return -12;
	}
    
	*eq = 0;

	while(*q) {
		if(*q == '"') {
			*q = 0;
			break;
		}

		q++;
	}

	add_attribute(elt, create_attribute(p, *val ? val : ""));

	return *q;
}

/* INTERNAL */
int comment_end_token(stream_t * stream) {
	int c1;
    c1 = get_c(stream);
    if(c1 < 0)
	   return c1;

    if(c1 == '-') {
 	   c1 = get_c(stream);
	   if(c1 < 0)
	      return c1;
			  

	   if( c1 == '>') {
		  return 1;
	   } else {
	 	 unget_c(stream, 1);
		 unget_c(stream, 1);
	   }
	} else {
	  unget_c(stream, 1);
	}

	return 0;
}

/*
 *
 * Parse comments 
 * 
 */
int parse_comment(stream_t * stream, xml_node * parent) {

  char * comment = NULL;
  xml_node * node;

  int c = read_text(stream, '-', comment_end_token, &comment);
  if(c == -10) {
    RAISE_ERROR(c, -10, stream, "Error while looking for comment end char for %s", parent->name)
  } else if(c == -11) {
    RAISE_ERROR(c, -11, stream, "Comment size too big for %s", parent->name)
  }

  if(comment && *comment) {
     node = create_comment(comment);
     add_childorsibling(parent, node);
  }

    

#ifdef DEBUG
  if(stream->config.nodeflush) {
 	 print(node, stream->config.fp, 0);
  }
#endif

  return 0;
}

/* INTERNAL */
int scan_comment(stream_t * stream) {

  int c1, c2 ,c;
  c1 = c2 = c = 0;

  for(;;) {
		c2 = c1;
		c1 = c;
		c = get_c(stream);
		if ( c < 0) return -4;
		
        if(c == '>' && c1 =='-' && c2 =='-') 
			return c;
  }
  
  /* to shut the compiler */
  return 0;
}


/* Use print to print a tree from a given node */
void print(xml_node * node, void *fp, int depth) {
  FILE * file = (FILE *)fp;

  int i = 0;
  if( node == NULL || file == NULL) {
	  return;
  }

  for(; i < depth; i++) {
       fprintf(file, "%c", '\t');
  }

  if(node->type == ELEMENT) {
    xml_element * element = node;

    fprintf(file, "<%s", element->name);
    if(element->attributes) {
	   xml_attribute * p = element->attributes;
       for (p =  element->attributes; p; p = p->next) {
	 	  fprintf(file, " %s=\"%s\"", p->name, p->value);
	  }
	}
	if(node->child)
      fprintf(file, "%c\n", '>');
	else {
      fprintf(file, "%s\n", "/>");
	}
  } else if ( node->type == CDATA) {
     fprintf(file, "<![CDATA[%s\n]]>", node->text);
  } else if (node->type == COMMENT) {
     fprintf(file, "<!--%s-->", node->text);
  } else if (node->type == TEXT) {/* text */
     fprintf(file, "%s", node->text);
  } else if (node->type == ENTITY) {
     fprintf(file, "<!%s %s>\n", node->name, node->text);
  } else if (node->type == PI) {
     fprintf(file, "<?%s %s?>\n", node->name, node->text);
  } else if (node->type == DOCUMENT) {
      --depth;
  }

  /* No work if this is a Document.
   */

  /* Recurse on child */
  print(node->child, file, depth + 1);

  if(node->child && (node->type == ELEMENT)) {
    int j = 0;
	for(; j < depth; j++) {
       fprintf(file, "%c", '\t');
	}
    fprintf(file, "</%s>\n", node->name);
  } 

  /* Recurse on sibling */
  print(node->sibling, file, depth);

  fflush(file);
  
}

/* Use this to remove attribute from an element */
void remove_attribute(xml_element * element,
						      char * name) {
	if( element && name ) {
        xml_attribute * p, *q;
		for( q = p = element->attributes; 
		     p; 
			 q = p, p = p->next) {

				 if(strcmp( p->name, name) == 0) {
					 if(q == p) {
                        element->attributes = p->next;
					 } else {
					    q->next = p->next;
					 }
				 }
		}
	 }
}

/* Use this to add attribute from an element */
void add_attribute(xml_element * element,
				   xml_attribute * attrib) {

	if( element && attrib ) {
		if(element->attributes) {
          xml_attribute * p  = element->attributes;

          while(p) {
			  if(strcmp(p->name, attrib->name) == 0) {
				  if(p->value) free(p->value);
			      p->value = attrib->value;
				  return;
			  }
			  p = p->next;
		  }

  		  attrib->next = element->attributes;
		}

  	    element->attributes = attrib;
	}
}

/* Process text replaces &<ref>; tokens with appropriate characters */
char * process_text(char * value) {
	char * buf = NULL;

	if(value) {
        int len = strlen(value);
		char * p = value;
		char * q = NULL;
	
		while( *p ) {
			char c = *p++;
            
			/* Relaxed estimate for length */
			if(c == '"' || 
			   c == '<' ||
			   c =='>'  ||
			   c == '\'' ||
			   c == '&')
			   len += 5;
		}
        
        p = value;
		buf = malloc(len+1);
		q = buf;

        while( *p ) {
			char c = *p++;
			if(c == '"' || c == '\'') {
              *q++ = '&';
			  *q++ = 'q';
			  *q++ = 'u';
              *q++ = 'o';
			  *q++ = 't';
			  *q++ = ';';
			} else if(c == '`') {
			  *q++ = '&';
			  *q++ = 'a';
			  *q++ = 'p';
              *q++ = 'o';
			  *q++ = 's';
			  *q++ = ';';
           	} else if( c == '&') {
  			  if(*p && *(p+1) && *(p+2)) {
				if(!memcmp(p, "lt;", 3) || !memcmp(p, "gt;", 3)) {
					*q++ = '&';
					*q++ = *p;
					*q++ = *(p+1);
					*q++ = *(p+2);
					p = p + 3;
					continue;
				} else if (*(p+3) && !memcmp(p, "amp;", 4)) {
					*q++ = '&';
					*q++ = 'a';
					*q++ = 'm';
					*q++ = 'p';
					*q++ = ';';
					p = p + 4;
					continue;
				} else if (*(p + 3) && *(p + 4)) {
					if(!memcmp(p, "quot;", 5) || !memcmp(p, "apos;", 5)) {
					   *q++ = '&';
					   *q++ = *p;
					   *q++ = *(p+1);
					   *q++ = *(p+2);
					   *q++ = *(p+3);
					   *q++ = *(p+4);
					   p = p + 5;
					   continue;
					}
				}
			  }

			  *q++ = '&';
			  *q++ = 'a';
			  *q++ = 'm';
              *q++ = 'p';
			  *q++ = ';';
			} else if (c == '<') {
			  *q++ = '&';
			  *q++ = 'l';
			  *q++ = 't';
			  *q++ = ';';
			} else if (c == '>') {
			  *q++ = '&';
			  *q++ = 'g';
			  *q++ = 't';
			  *q++ = ';';
			} else if (c == '\r') {
                if(*p && *p == '\n')  {
                    *q++ = '\n';
                     ++p;
                     continue;
                }
            } else {
              *q++ = c;
			}

		} /* while */

		*q = 0;
	  }

	return buf;
}

/* Use this to normalize the tree. This will merge adjacent text nodes */
void normalize(xml_node * node) {
    if(node) {
        xml_node * child = node->child;

        while(child) {
            if(child->type == TEXT) {
                xml_node * sibling = child->sibling;

                if(sibling && sibling->type == TEXT) {
                    if(child->text && sibling->text) {
                        char * text = malloc(strlen(child->text) + 
                                             strlen(sibling->text) + 1);
                        if(text) {
                           strcpy(text, child->text);
                           free(child->text);
                           strcat(text, sibling->text);
                           free(sibling->text);

                           child->text = text;
                           sibling->text = NULL;

                           remove_childorsibiling(node, sibling);
                           destroy_node(sibling);

                           /* restart from the current child */

                           continue;
                        }
                    }
                }
            }

            /* advance */
            child = child->sibling;

        }/* while */
    }/* node */
}

/* Create an attribute with a name and value  */
xml_attribute * create_attribute(char * name, 
								 char * value) {

   xml_attribute * attrib = NULL;

   if(!name) return NULL;
   attrib = (xml_attribute*)calloc(1, sizeof(xml_attribute));
   if(attrib) {
	  attrib->name = malloc(strlen(name) + 1);
	  strcpy(attrib->name, name);

	  attrib->value = process_text(value);
   }

   return attrib;

}


/* use this to add a child or sibling to a node. This is 
   the most preferred way to add a child or sibling element
 */
void  add_childorsibling(xml_node * parent, 
                         xml_node * childorsibling) {

	if(parent &&  childorsibling) {
		if(parent->child == NULL) {
		   parent->child = childorsibling;
		} else {
		  xml_node * p = parent->child; 
		  xml_node * q = p;

		  while(p) {
			 q = p;
			 p = p->sibling;
		  }
		  q->sibling = childorsibling;
		}

		childorsibling->sibling = NULL;
        childorsibling->parent = parent;
	}
}

/* Use this to remove child or sibling */
xml_node * remove_childorsibiling(xml_node * parent, 
                                  xml_node * childorsibling) {
 
	if(parent &&  childorsibling) {
		if(parent->child == NULL) {
		   return NULL;
		} else {
		  xml_node * p = parent->child; 
          xml_node * q = p;

		  while(p) {
 		       if(p == childorsibling) {
                   if(p == parent->child) {
                     parent->child = p->sibling;
                   } else {
                     q->sibling = p->sibling;
                   }
                   return p;
               }
               q = p;
               p = p->sibling;
		  }
        }
	}

    return NULL;
}

/* INTERNAL - not used */
int is_equal(xml_node * child1, xml_node * child2) {

	if(child1 == NULL && child2 == NULL)
		return 1L;

    if(child1->type != child2->type)
		return 0L;

	if(child1->type == TEXT) {
        return (strcmp(child1->text, child2->text) == 0);
	} else {
		/* element comparison */
		if(strcmp(child1->name, child2->name) == 0) {
			return 1L;
		}

		/* TODO */
	}

	return 0L;
}

/* create text node */
xml_node * create_text(char * text) {
   xml_node * n = (xml_node *)malloc(sizeof(xml_node));

   if(n) {
	   n->sibling = n->child = NULL;
	   if(text) {
	      n->text = process_text(text);
       }

       n->parent = NULL;
	   
	   n->type = TEXT;
   }

   return n;
}

/* Create a PI node */
xml_element * create_PI(char * name) {
  /* kindda hacky */
  /* PIs also have attributes etc. so we make give it element status */
  xml_element * pi = create_element(name);
  pi->type = PI;
  return pi;
}

/* Create the root doc */
xml_node * create_document() {
    return create_splnode(DOCUMENT, NULL);
}

/* Create a CDATAsection node */
xml_node * create_CDATA(char * text) {
	return create_splnode(CDATA, text);
}

/* Create a Comment node. Comments can be first class citizens */
xml_node * create_comment(char * text) {
	return create_splnode(COMMENT, text);
}

/* Create an entity */
xml_node * create_entity(char * name) {
   xml_node * n = (xml_node *)malloc(sizeof(xml_node));

   if(n) {
	   n->sibling = n->child = NULL;
       n->text = NULL;
   	   if(name) {
	      n->name = malloc(strlen(name) + 1);
	      strcpy(n->name, name); 
	   } else {
		  n->name = NULL;
	   }
	   n->type = ENTITY;
       n->parent = NULL;
   }

   return n;
}

/* INTERNAL */
xml_node * create_splnode(xml_type type, char * text) {
   xml_node * n = (xml_node *)malloc(sizeof(xml_node));

   if(n) {
	   n->sibling = n->child = NULL;
	   n->text = process_text(text);
       n->name = NULL;
	   n->type = type;
       n->parent = NULL;
   }

   return n;
}


/* Use this to create an element node */
xml_element * create_element(char * name) {
    
   xml_element * e = (xml_element *)malloc(sizeof(xml_element));
   if(e) {
	   e->sibling = e->child = NULL;
	   e->text = NULL;
	   e->attributes = NULL;
       e->parent = NULL;
       
	   e->type = ELEMENT;

	   if(name) {
	      e->name = malloc(strlen(name) + 1);
	      strcpy(e->name, name); 
	   }
   }

   return e;
}


/* Use this to select a set of nodes using a given xpath.
   pcount --> should point to an int that will hold the count of nodes returned
   xpathstr --> is XPath.
   Note that only a limited XPath syntax( very common one) is supported and tested! 
*/
xml_node** select_nodes(xml_node* current, 
                        int *pcount, 
                        char * xpathstr) {
    char * p;
    char * q; 
    xml_node **nodearr;

    int count  = 1;
    char *xpath = NULL;

    if(xpathstr == NULL) {
       return NULL;
    }

    xpath = malloc(strlen(xpathstr) + 1);
    strcpy(xpath, xpathstr);
    q = xpath;
    
    if(*q == '/') {
       xml_node * c = current;
       while(c) {
             current = c;
             c = c->parent;
       }
       q++;
    }

    nodearr = malloc(sizeof(xml_node *));
    nodearr[0] = current;
    
    for(;q && *q;) {
        xml_node **tmparr;
        p = strchr(q, '/');

        if(p) {
          *p = 0;
        }

        tmparr = select(nodearr, &count, q);
        if(tmparr) {
            free(nodearr);
            nodearr = tmparr;

        }
        q = p ? p + 1 : NULL;
    }

    *pcount = count;
    free(xpath);
    return nodearr;
}

/* Returns an attribute's value given the key and a node */
char * get_attribute(xml_node * node, 
                     char * name) {
    if(node && name) {
        xml_attribute * attrib = node->attributes;
        while(attrib) {
            if(!strcmp(attrib->name, name)) {
               return attrib->value;  
            }
            attrib = attrib->next;
        }
    }

    return NULL;
}

/* Returns an attribute's value given an xpath */
char * get_attrib_value(xml_node * current, 
                        char *xpath) {
   int count;
   xml_node ** nodes = select_nodes(current, &count, xpath);
   if(count > 0) {
       char * at = strchr(xpath, '@');
       if(at && *(at+1)) {
          return get_attribute(nodes[0], at + 1);
       }
   }

   return NULL;
}

/*
 * INTERNAL
 * Selects a node given an partial path ( not XPath)
 * 
 */
xml_node** select(xml_node ** current, 
                  int *pcount, 
                  char * path) {
    xml_node **nodearr = NULL;
    int maxindex = 0;
    int index = 0;
    int j = 0;
    xml_node * node;

    /* pcount is input and output */
    int nodecount = *pcount;

    if(!current || !path) {
        *pcount = 0;
        return current;
    }

    if(path[0] == '@') { /* attribute */
          /* one should search for attributes in the nodes themselves */
          xml_node * snode = NULL;
          char * p = &path[1];
          char * name, *value = NULL;
          parse_name_value(p, &name, &value);

          for(j = 0; j < nodecount; j++) {
            node = current[j];

            if(find_attribute(node, name, value)) {
               if(index == maxindex) {
                 xml_node** tmparr = realloc(nodearr, (maxindex + 5) * sizeof(xml_node *));
                 if(!tmparr) {
                       /* Warn for memory error */
                      return nodearr;
                 }

                 nodearr = tmparr;
                 maxindex += 4;
               }

               nodearr[index++] = node;
            }
          }

          if(name) free(name);
          if(value) free(value);

    } else {
        for(j = 0; j < nodecount; j++) {
            node = current[j]->child;

            while(node) {
               int flag = !strcmp(node->name, path);
               if(!flag) {
                   char * col = strchr(node->name, ':');
                   if(col && *(col + 1)) {
                      flag = !strcmp(col+1, path);
                   }
               }

               if(flag) {
                  if(index == maxindex) {
                    xml_node** tmparr = realloc(nodearr, (maxindex + 5) * sizeof(xml_node *));
                    if(!tmparr) {
                       /* Warn for memory error */
                       return nodearr;
                    }

                    nodearr = tmparr;
                    maxindex += 4;
                  }

                  nodearr[index++] = node;
               }

               node = node->sibling;
            }
        }
    }

    *pcount = index;
    return nodearr;
}

void parse_name_value(char *p, 
                      char** name, 
                      char** value) {
    char * q = strchr(p, '=');
    *value = *name = NULL;
    if( q && *(q + 1) &&(*p != *q) ) {
        *q = 0;
        *value = (char *)malloc(strlen(q + 1) + 1);
        strcpy(*value, q + 1);
    }

    *name = (char *)malloc(strlen(p) + 1);
    strcpy(*name, p);
}

xml_attribute * find_attribute(xml_node * node,
                               char * name , 
                               char * value) {

    if(node != NULL) {
        xml_attribute * attrib = node->attributes;

        while(attrib) {
            if(name && !strcmp(attrib->name, name)) {
                if(value) {
                  if(!strcmp(attrib->value, value))
                      return attrib; 
                } else {
                   return attrib;
                }
            }

            attrib = attrib->next;
        }
    }

    return NULL;
}


char * get_text(xml_node * current) {
    if(current && current->type == ELEMENT) {
        xml_node *node = current->child;

        while(node && node->type != TEXT) {
           node = node->sibling;
        }

        if(node) {
           return node->text;
        }
    }

    return NULL;
}



