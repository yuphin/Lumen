#ifndef BDA_GLSL
#define BDA_GLSL

#define BUFFER_REF(Name, T) \
	layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Name { T d[]; }
#define BUFFER_REF_RO(Name, T) \
	layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Name { T d[]; }
#define BUFFER_REF_SCALAR(Name, T) \
	layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Name { T d; }
#define BUFFER_REF_SCALAR_RO(Name, T) \
	layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Name { T d; }

// Declares ref type <field>_ref and cached global pointer <field>_buf from <desc>.<field>_addr.
// The cached global is required for render graph shader inference
#define DESC_BUFFER(desc, field, T) \
	BUFFER_REF(field##_ref, T);     \
	field##_ref field##_buf = field##_ref(desc.field##_addr)
#define DESC_BUFFER_RO(desc, field, T) \
	BUFFER_REF_RO(field##_ref, T);     \
	field##_ref field##_buf = field##_ref(desc.field##_addr)
#define DESC_BUFFER_SCALAR(desc, field, T) \
	BUFFER_REF_SCALAR(field##_ref, T);     \
	field##_ref field##_buf = field##_ref(desc.field##_addr)
#define DESC_BUFFER_SCALAR_RO(desc, field, T) \
	BUFFER_REF_SCALAR_RO(field##_ref, T);     \
	field##_ref field##_buf = field##_ref(desc.field##_addr)

#define SCENE_BUFFER(field, T) DESC_BUFFER(scene_desc, field, T)
#define SCENE_BUFFER_RO(field, T) DESC_BUFFER_RO(scene_desc, field, T)
#define SCENE_BUFFER_SCALAR(field, T) DESC_BUFFER_SCALAR(scene_desc, field, T)
#define SCENE_BUFFER_SCALAR_RO(field, T) DESC_BUFFER_SCALAR_RO(scene_desc, field, T)

// Arrays: DEREF(field)[i], scalars: DEREF(field)
#define DEREF(field) field##_buf.d

#endif
