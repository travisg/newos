/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


/*
 * rldelf.c requires this function to be implemented on a per-cpu basis
 */
static
bool
relocate_image(image_t *image)
{
	(void)(image);

	return true;
}
