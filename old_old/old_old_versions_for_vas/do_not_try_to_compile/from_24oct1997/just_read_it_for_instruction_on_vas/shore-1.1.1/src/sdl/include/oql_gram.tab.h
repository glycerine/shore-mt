/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
typedef union
{
   // Declaration*  decl_ptr;
   // m_list_t<Declaration>* decl_list;

   // Sdl_Type* type_ptr;

   // ExprNode*     expr_ptr;
   // m_list_t<ExprNode>* expr_list;
	struct node * nodept; 
	UnRef<sdlDeclaration> declpt; 
	UnRef<sdlType> typept; 
	UnRef<sdlExprNode> exprpt; 
	enum Mode mode;
	Zone zone;
	TypeTag ttag;
	short code; 

   char*         string;
   Ql_tree_node* value;
   int 	 int_val;
} YYSTYPE;
#define	LASTPRED	258
#define	ASSIGN	259
#define	SELECT	260
#define	FROM	261
#define	WHERE	262
#define	FIELDPREC	263
#define	QUERYPREC	264
#define	DOTS	265
#define	OR	266
#define	AND	267
#define	DIFF	268
#define	LIKE	269
#define	INFEQUAL	270
#define	SUPEQUAL	271
#define	UNION	272
#define	EXCEPT	273
#define	INTERSECT	274
#define	tIN	275
#define	NOT	276
#define	DOT	277
#define	ARROW	278
#define	DEFINE	279
#define	USET	280
#define	OQL_ABS	281
#define	COUNT	282
#define	SUM	283
#define	OQL_MIN	284
#define	OQL_MAX	285
#define	AVG	286
#define	ELT	287
#define	FIRST	288
#define	LAST	289
#define	DISTINCT	290
#define	UNIQUE	291
#define	LISTOSET	292
#define	FLATTEN	293
#define	TUPLE	294
#define	EXISTS	295
#define	FORALL	296
#define	FOR	297
#define	SORT	298
#define	GROUP	299
#define	BY	300
#define	WITH	301
#define	CREATE	302
#define	DROP	303
#define	OPEN	304
#define	CLOSE	305
#define	DB	306
#define	tINDEX	307
#define	CLUSTERED	308
#define	OQL_ON	309
#define	ONLY	310
#define	USING	311
#define	UPDATE	312
#define	INSERT	313
#define	DELETE	314
#define	DESTROY	315
#define	INTO	316
#define	TO	317
#define	APPLY	318
#define	PLUS_EQUAL	319
#define	MINUS_EQUAL	320
#define	INTERFACE	321
#define	MODULE	322
#define	TYPEDEF	323
#define	STRUCT	324
#define	ENUM	325
#define	CONST	326
#define	IMPORT	327
#define	EXPORT	328
#define	USE	329
#define	AS	330
#define	ALL	331
#define	PERSISTENT	332
#define	TRANSIENT	333
#define	PUBLIC	334
#define	tPRIVATE	335
#define	PROTECTED	336
#define	KEY	337
#define	EXTENT	338
#define	ATTRIBUTE	339
#define	READONLY	340
#define	RELATIONSHIP	341
#define	INVERSE	342
#define	ORDER_BY	343
#define	OVERRIDE	344
#define	EXTERNAL	345
#define	CLASS	346
#define	POOL	347
#define	ORDERED_BY	348
#define	tOUT	349
#define	tINOUT	350
#define	RAISES	351
#define	DIRECT	352
#define	ONEWAY	353
#define	CONTEXT	354
#define	CASE	355
#define	DEFAULT	356
#define	SWITCH	357
#define	ARRAY	358
#define	SEQUENCE	359
#define	tREF	360
#define	tLREF	361
#define	tBAG	362
#define	tSET	363
#define	LIST	364
#define	MULTILIST	365
#define	DCOLON	366
#define	RSHIFT	367
#define	LSHIFT	368
#define	ANY	369
#define	OCTET	370
#define	LONG	371
#define	SHORT	372
#define	UNSIGNED	373
#define	DOUBLE	374
#define	BOOLEAN	375
#define	VOID	376
#define	tSTRING	377
#define	CHAR	378
#define	FLOAT	379
#define	INT	380
#define	TEXT	381
#define	INDEXABLE	382
#define	LEX_STRING_LIT	383
#define	LEX_INT_LIT	384
#define	LEX_CHAR_LIT	385
#define	LEX_FLOAT_LIT	386
#define	LEX_TRUE	387
#define	LEX_FALSE	388
#define	LEX_OQL_NIL	389
#define	LEX_ID	390
#define	EOI	391
#define	ILLEGAL_INPUT	392

